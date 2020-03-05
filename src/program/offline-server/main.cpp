#include <iostream>
#include <memory>
#include <event2/thread.h>
#include <controllers/system_controller.h>

#include "http/http_service.h"
#include "utils/ssl_utils.h"
#include "utils/log.h"
#include "dao/client.h"

#include "offline_config.h"

#include "redis/redis_manager.h"
#include "redis/redis_manage_timer.h"

#include "redis/hiredis_client.h"
#include "fiber/fiber_timer.h"

#include "offline_server_controller.h"
#include "registers/offline_register.h"
#include "push/push_service.h"
#include "dispatcher/offline_dispatcher.h"
#include "offline_server.h"

using namespace bcm;

static void globalInit(OfflineConfig& config)
{
    // for thread safe
    evthread_use_pthreads();

    Log::init(config.log, "offline-server");

    bool storeOk = dao::initialize(config.dao);
    if (storeOk) {
        LOGI << "init dao success!";
    } else {
        LOGE << "init dao error!";
        exit(-1);
    }
}

static void globalClean()
{
    Log::flush();
}

int main(int argc, char** argv)
{
    OfflineOptions options = OfflineOptions::parseCmd(argc, argv);
    auto& config = options.getConfig();

    globalInit(config);

    nlohmann::json  tmpJson;
    to_json(tmpJson, config);
    LOGI << "service start! config: " << tmpJson.dump() ;

    auto sslCtx = SslUtils::loadServerCertificate(config.http.ssl.certFile,
                                                  config.http.ssl.keyFile, config.http.ssl.password);
    if (sslCtx == nullptr) {
        LOGE << "load ssl certificate failed!  cert file: " << config.http.ssl.certFile
                << ", key file: " << config.http.ssl.keyFile;
        exit(-1);
    }
    
    // get redisdb config
    if (config.groupRedis.find(config.offlineSvr.redisPartition) == config.groupRedis.end()) {
        LOGE << "load groupRedis config failed! not found partition: " << config.offlineSvr.redisPartition;
        exit(-1);
    }
    
    std::map<int32_t, RedisConfig> redisDbHosts;
    for (const auto& itDb : config.groupRedis[config.offlineSvr.redisPartition]) {
        int32_t  redisId = -1;
        try {
            redisId = std::stoi(itDb.first);
        } catch (std::exception& e) {
            LOGE << "load redisDb config failed! redis partition: " << config.offlineSvr.redisPartition
                 << ", redis index is not int32_t: " << itDb.first
                 << ", exception caught: " << e.what();
            exit(-1);
        }
        redisDbHosts[redisId] = itDb.second;
    }

    if (!RedisClientSync::OfflineInstance()->setRedisConfig(redisDbHosts)) {
        LOGE << "redisDb config format failed!";
        exit(-1);
    }
    
    // redis for group/offline
    if (!RedisDbManager::Instance()->setRedisDbConfig(config.groupRedis)) {
        LOGE << "groupRedis config error";
        exit(-1);
    }
    auto redisFiberTimer = std::make_shared<FiberTimer>();
    auto redisManageTimer = std::make_shared<RedisManageTimer>(RedisDbManager::Instance());
    
    RedisClientAsync::Instance()->setRedisConfig(config.redis);
    RedisClientAsync::Instance()->startAsyncRedisThread();

    auto fiberTimer = std::make_shared<FiberTimer>();

    auto accountsManager = std::make_shared<AccountsManager>();
    auto groupUsers = dao::ClientFactory::groupUsers();
    auto sysMsgs = dao::ClientFactory::sysMsgs();
    auto authenticator = std::make_shared<Authenticator>(accountsManager);

    auto offlineServiceRegister = std::make_shared<OfflineServiceRegister>(config.redis[0],
                                                                           config.http, config.offlineSvr.pushType);
    // TODO: use offlineDispatcher in OfflineService
    // auto offlineDispatcher = std::make_shared<OfflineDispatcher>(offlineServiceRegister);

    auto pushService = std::make_shared<PushService>(accountsManager,
                       config.redis[0], config.apns, config.fcm, config.umeng);

    //
    auto offlineService = std::make_shared<OfflineService>(config, accountsManager,
                                                           offlineServiceRegister, redisDbHosts, pushService);
    fiberTimer->schedule(offlineService, config.offlineSvr.pushRoundInterval, false);
    fiberTimer->schedule(offlineServiceRegister, OfflineServiceRegister::kKeepAliveInterval, false);
    redisFiberTimer->schedule(redisManageTimer, RedisManageTimer::redisManageTimerInterval, true);
    
    auto httpRouter = std::make_shared<HttpRouter>();

    auto offlinePushController = std::make_shared<OfflinePushController>(pushService, config.offlineSvr);
    auto innerSystemController = std::make_shared<InnerSystemController>(accountsManager, pushService, sysMsgs,
                                                                         config.sysmsg);
    
    httpRouter->add(offlinePushController);
    httpRouter->add(innerSystemController);

    auto service = std::make_shared<HttpService>(sslCtx, httpRouter, authenticator, config.http.concurrency, nullptr);
    service->run(config.http.host, config.http.port);

    service->wait();

    //
    fiberTimer->cancel(offlineService);
    fiberTimer->cancel(offlineServiceRegister);
    redisFiberTimer->cancel(redisManageTimer);
    
    sleep(1);   // waiting for fiberTimer->cancel
    
    globalClean();
    return 0;
}
