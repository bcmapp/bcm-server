#include <iostream>
#include <memory>
#include <event2/thread.h>

#include "http/http_service.h"
#include "utils/ssl_utils.h"
#include "utils/log.h"
#include "dao/client.h"
#include "auth/authenticator.h"
#include "auth/turntoken_generator.h"
#include "dispatcher/dispatch_manager.h"

#include "config/bcm_options.h"
#include "config/dao_config.h"
#include "config/service_config.h"
#include "config/bcm_metrics_config.h"

#include "store/accounts_manager.h"
#include "store/messages_manager.h"
#include "store/keys_manager.h"
#include "store/contact_token_manager.h"

#include "controllers/echo_controller.h"
#include "controllers/account_controller.h"
#include "controllers/group_msg_controller.h"
#include "controllers/message_controller.h"
#include "controllers/keys_controller.h"
#include "controllers/keep_alive_controller.h"
#include "controllers/device_keepalive_controller.h"
#include "controllers/contact_controller.h"
#include "controllers/profile_controller.h"
#include "controllers/attachment_controller.h"
#include "controllers/system_controller.h"
#include "controllers/group_manager_controller.h"
#include "controllers/opaque_data_controller.h"
#include "controllers/device_controller.h"

#include "push/push_service.h"
#include "redis/hiredis_client.h"
#include "fiber/fiber_timer.h"
#include "registers/lbs_register.h"
#include "registers/imservice_register.h"
#include "registers/offline_register.h"
#include "group/group_msg_service.h"
#include <metrics_client.h>
#include <metrics_log.h>
#include "metrics/onlineuser_metrics.h"
#include "redis/redis_manager.h"
#include "redis/redis_manage_timer.h"
#include "redis/online_redis_manager.h"
#include "limiters/configuration_manager.h"
#include "limiters/limiter_config_update.h"
#include "limiters/limiter_executor.h"
#include "limiters/limiter_globals.h"

#ifndef NDEBUG
#include "filters/magic_status_code_filter.h"
#endif

using namespace bcm;
using namespace bcm::metrics;


static void globalInit(BcmConfig config)
{
    // for thread safe
    evthread_use_pthreads();

    Log::init(config.log, "bcm-server");

    bool storeOk = dao::initialize(config.dao);
    if (storeOk) {
        LOGI << "init dao success!";
    } else {
        LOGE << "init dao error!";
        std::cerr << "init dao error!" << std::endl;
        exit(-1);
    }
}

static void globalClean()
{
    Log::flush();
}

void metricsLogCallback(MetricsLogLevel level, const std::string& str)
{
    switch (level) {
        case METRICS_LOGLEVEL_TRACE:
            BOOST_LOG_SEV(BcmLogger::get(), LOGSEVERITY_TRACE) << boost::this_fiber::get_id() << "|" << str;
            break;
        case METRICS_LOGLEVEL_DEBUG:
            BOOST_LOG_SEV(BcmLogger::get(), LOGSEVERITY_DEBUG) << boost::this_fiber::get_id() << "|" << str;
            break;
        case METRICS_LOGLEVEL_INFO:
            BOOST_LOG_SEV(BcmLogger::get(), LOGSEVERITY_INFO) << boost::this_fiber::get_id() << "|" << str;
            break;
        case METRICS_LOGLEVEL_WARN:
            BOOST_LOG_SEV(BcmLogger::get(), LOGSEVERITY_WARN) << boost::this_fiber::get_id() << "|" << str;
            break;
        case METRICS_LOGLEVEL_ERROR:
            BOOST_LOG_SEV(BcmLogger::get(), LOGSEVERITY_ERROR) << boost::this_fiber::get_id() << "|" << str;
            break;
        case METRICS_LOGLEVEL_FATAL:
            BOOST_LOG_SEV(BcmLogger::get(), LOGSEVERITY_FATAL) << boost::this_fiber::get_id() << "|" << str;
            break;
        default:
            break;
    }
}

int main(int argc, char** argv)
{
    BcmOptions::getInstance()->parseCmd(argc, argv);
    auto& config = BcmOptions::getInstance()->getConfig();

    globalInit(config);

    LOGI << "service start!";
    LOGI << "encryptSender config, plainUidSupport: " << config.encryptSender.plainUidSupport;

    auto sslCtx = SslUtils::loadServerCertificate(config.http.ssl.certFile,
                                                  config.http.ssl.keyFile,
                                                  config.http.ssl.password);
    if (sslCtx == nullptr) {
        LOGE << "load ssl certificate failed!";
        std::cerr << "load ssl certificate failed!" << std::endl;
        exit(-1);
    }

    // import metrics sdk
    MetricsLogger::instance()->registLogFunction(&metricsLogCallback);
    MetricsConfig metricsConfig;
    BCMMetricsConfig::copyToMetricsConfig(config.bcmMetricsConfig, metricsConfig);
    MetricsClient::Init(metricsConfig);

    RedisClientSync::Instance()->setRedisConfig(config.redis);

    // redis for group/offline
    if (!RedisDbManager::Instance()->setRedisDbConfig(config.groupRedis)) {
        LOGE << "groupRedis config error";
        exit(-1);
    }
    // redis for sub/pub
    if (!OnlineRedisManager::Instance()->init(config.onlineRedis)) {
        LOGE << "online redis manager init fail";
        exit(-1);
    }
    OnlineRedisManager::Instance()->start();

    auto fiberTimer = std::make_shared<FiberTimer>();

    auto challenges = dao::ClientFactory::signupChallenges();
    auto contacts = dao::ClientFactory::contacts();

    auto accountsManager = std::make_shared<AccountsManager>();
    auto messagesManager = std::make_shared<MessagesManager>();
    auto keysManager = std::make_shared<KeysManager>();
    
    auto redisFiberTimer = std::make_shared<FiberTimer>();
    auto redisManageTimer = std::make_shared<RedisManageTimer>(RedisDbManager::Instance());

    auto authenticator = std::make_shared<Authenticator>(accountsManager);
    auto turnTokenGenerator = std::make_shared<TurnTokenGenerator>(config.turn);

    auto lbsRegister = std::make_shared<LbsRegister>(config.lbs, config.http);
    auto imServiceRegister = std::make_shared<IMServiceRegister>(config.redis[0], config.http);  // w todo 修改 redis server 的发现机制
    auto offlineRegister = std::make_shared<OfflineServiceRegister>(config.redis[0]);
    auto offlineDispatcher = std::make_shared<OfflineDispatcher>(offlineRegister);
    
    auto dispatchManager = std::make_shared<DispatchManager>(config.dispatcher,
                                                             messagesManager,
                                                             offlineDispatcher,
                                                             contacts,
                                                             config.encryptSender);

    auto groupMsgService = std::make_shared<GroupMsgService>(config.redis[0], dispatchManager, config.noise);

    for (const std::string& key : imServiceRegister->getRegisterKeys()) {
        groupMsgService->addRegKey(key);
    }

    LimiterConfigurationManager::getInstance()->initialize();
    std::shared_ptr<LimiterExecutor> validator = std::make_shared<LimiterExecutor>();

    auto httpRouter = std::make_shared<HttpRouter>();
    auto upgraderRouter = std::make_shared<HttpRouter>();
    auto deviceUpgraderRouter = std::make_shared<HttpRouter>();
    httpRouter->registerObserver(LimiterGlobals::getInstance());
    upgraderRouter->registerObserver(LimiterGlobals::getInstance());
    deviceUpgraderRouter->registerObserver(LimiterGlobals::getInstance());
    
    auto echoController = std::make_shared<EchoController>();
    auto accountController = std::make_shared<AccountsController>(accountsManager, challenges, turnTokenGenerator,
                                                                  dispatchManager, keysManager, config.challenge);
    auto deviceController = std::make_shared<DeviceController>(accountsManager, dispatchManager, keysManager, config.multiDeviceConfig);
    auto groupMsgController = std::make_shared<GroupMsgController>(groupMsgService, config.encryptSender, 
                                                                  config.sizeCheck);
    
    auto messageController = std::make_shared<MessageController>(accountsManager,
                                                                 offlineDispatcher,
                                                                 dispatchManager,
                                                                 config.encryptSender,
                                                                 config.multiDeviceConfig,
                                                                 config.sizeCheck);
    auto keysController = std::make_shared<KeysController>(accountsManager, keysManager, config.sizeCheck);
    auto keepAliveController = std::make_shared<KeepAliveController>(dispatchManager, groupMsgService);
    auto deviceKeepaliveController = std::make_shared<DeviceKeepaliveController>(dispatchManager);
    
    auto contactController = std::make_shared<ContactController>(contacts,
                                                                 accountsManager,
                                                                 dispatchManager,
                                                                 offlineDispatcher,
                                                                 config.sizeCheck);
    auto profileController = std::make_shared<ProfileController>(accountsManager, authenticator);
    auto attachmentController = std::make_shared<AttachmentController>(config.s3Config, accountsManager);
    
    auto groupManagerController = std::make_shared<GroupManagerController>(groupMsgService,
                                                                           accountsManager,
                                                                           dispatchManager,
                                                                           config.multiDeviceConfig,
                                                                           config.groupConfig);
    auto opaqueDataController = std::make_shared<OpaqueDataController>();

    httpRouter->add(echoController);
    httpRouter->add(accountController);
    httpRouter->add(deviceController);
    httpRouter->add(groupMsgController);
    httpRouter->add(messageController);
    httpRouter->add(keysController);
    httpRouter->add(contactController);
    httpRouter->add(profileController);
    httpRouter->add(attachmentController);
    httpRouter->add(groupManagerController);
    httpRouter->add(opaqueDataController);

    #ifndef NDEBUG
    auto magicStatusCodeFilter = std::make_shared<MagicStatusCodeFilter>();
    httpRouter->add(magicStatusCodeFilter);
    #endif

    if (config.http.websocket) {
        upgraderRouter->add(keepAliveController);
        upgraderRouter->add(messageController);
        deviceUpgraderRouter->add(deviceKeepaliveController);
    }
    #if 0
    LimiterGlobals::getInstance()->dump();
    abort();
    #endif

    auto service = std::make_shared<HttpService>(sslCtx, httpRouter, authenticator, config.http.concurrency, validator);
    if (config.http.websocket) {
        service->enableUpgrade(std::make_shared<WebsocketService>("/v1/websocket", sslCtx, upgraderRouter,
                                                                  authenticator, dispatchManager, 0, validator, WebsocketService::TOKEN_AUTH));
        service->enableUpgrade(std::make_shared<WebsocketService>("/v1/websocket/device", sslCtx, deviceUpgraderRouter,
                                                                  authenticator, dispatchManager, 0, validator, WebsocketService::REQUESTID_AUTH));
    }

    // report online user to metrics
    // use reportIntervalInMs / 2 to make sure report during metrics report interval
    OnlineUserMetrics onlineUserMetricsReporter(dispatchManager,
                                                config.bcmMetricsConfig.reportIntervalInMs / 2);
    onlineUserMetricsReporter.start();

    dispatchManager->start();
    LimiterConfigurationManager::getInstance()->reloadConfiguration();
    service->run(config.http.host, config.http.port);
    std::shared_ptr<LimiterConfigUpdate> limiterConfigUpdater = std::make_shared<LimiterConfigUpdate>();
    fiberTimer->schedule(lbsRegister, LbsRegister::kKeepAliveInterval, true);
    fiberTimer->schedule(imServiceRegister, IMServiceRegister::kKeepAliveInterval, false);
    fiberTimer->schedule(offlineRegister, OfflineServiceRegister::kKeepAliveInterval, true);
    fiberTimer->schedule(limiterConfigUpdater, config.limiterConfig.configUpdateInterval, false);
    redisFiberTimer->schedule(redisManageTimer, RedisManageTimer::redisManageTimerInterval, true);

    service->wait();
    fiberTimer->cancel(lbsRegister);
    fiberTimer->cancel(imServiceRegister);
    fiberTimer->cancel(offlineRegister);
    fiberTimer->cancel(limiterConfigUpdater);
    redisFiberTimer->cancel(redisManageTimer);
    LimiterConfigurationManager::getInstance()->uninitialize();
    globalClean();
    return 0;
}
