#include "push_service.h"
#include "fcm_notification.h"
#include "fcm_client.h"
#include "umeng_notification.h"
#include "umeng_client.h"
#include "apns_notification.h"
#include "apns_service.h"
#include "apns_qos_mgr.h"
#include "fiber/asio_round_robin.h"
#include "fiber/fiber_pool.h"
#include "store/accounts_manager.h"
#include "utils/log.h"
#include "utils/thread_utils.h"
#include "dispatcher/dispatch_address.h"

#include "proto/dao/account.pb.h"
#include "proto/dao/device.pb.h"

#include <boost/fiber/all.hpp>
#include <chrono>

#include "redis/redis_manager.h"

namespace bcm {
namespace push {
typedef boost::asio::io_context io_context;
typedef boost::system::error_code error_code;
// -----------------------------------------------------------------------------
// Section: RetryContext
// -----------------------------------------------------------------------------
class RetryContext {
    int32_t m_maxDelayMillis;
    int32_t m_maxRetries;
    int32_t m_delayMillis;
    int32_t m_retries;

public:
    RetryContext(int32_t maxDelayMillis, int32_t maxRetries)
        : m_maxDelayMillis(maxDelayMillis), m_maxRetries(maxRetries)
        , m_delayMillis(0), m_retries(0) {}

    bool willRetry() const
    {
        if ( (m_maxDelayMillis == 0) && (m_delayMillis == 0) ) {
            return false;
        } else if ( (m_maxDelayMillis > 0)
                            && (m_delayMillis >= m_maxDelayMillis) ) {
            return false;
        } else if ( (m_maxRetries > 0) && (m_retries >= m_maxRetries) ) {
            return false;
        } else {
            return true;
        }
    }

    int32_t getRetryCount() const
    {
        return m_retries;
    }

    RetryContext& addDelayMillis(int32_t delayMillis)
    {
        m_delayMillis += delayMillis;
        return *this;
    }

    RetryContext& increaseRetryCount()
    {
        ++m_retries;
        return *this;
    }
};

// -----------------------------------------------------------------------------
// Section: Backoff
// -----------------------------------------------------------------------------
class Backoff {
public:
    virtual ~Backoff() = default;
    virtual int32_t delayMillis(const RetryContext& context) = 0;
};

class ExponentialDelayBackoff : public Backoff {
    int32_t m_initialDelayMillis;
    double  m_multiplier;
public:
    ExponentialDelayBackoff(int32_t initialDelayMillis, double multiplier)
        : m_initialDelayMillis(initialDelayMillis), m_multiplier(multiplier) {}

    int32_t delayMillis(const RetryContext& context) override
    {
        return (int32_t) (m_initialDelayMillis *
                          pow(m_multiplier, context.getRetryCount() - 1));
    }
};

class UniformRandomBackoff : public Backoff {
    // static std::random_device kRandomDev;
    static const int32_t kDefaultRandomRangeMillis;
    std::random_device m_rd;
    std::default_random_engine m_re;
    Backoff* m_target;
public:
    explicit UniformRandomBackoff(Backoff* backoff)
        : m_rd(), m_re(m_rd()), m_target(backoff) {}

    virtual ~UniformRandomBackoff()
    {
        if (m_target != nullptr) {
            delete m_target;
        }
    }

    int32_t delayMillis(const RetryContext& context) override
    {
        int32_t initialDelay = m_target->delayMillis(context);
        int32_t randomDelay = addRandomJitter(initialDelay);
        return std::max(randomDelay, 0);
    }

    int32_t addRandomJitter(int32_t initialDelay)
    {
        double randomDouble =
            std::uniform_real_distribution<double>(0, 1)(m_re);
        double uniformRandom =
            (1 - randomDouble * 2) * kDefaultRandomRangeMillis;
        return (int32_t)(initialDelay + uniformRandom);
    }
};

// std::random_device UniformRandomBackoff::kRandomDev;
const int32_t UniformRandomBackoff::kDefaultRandomRangeMillis = 100;

// -----------------------------------------------------------------------------
// Section: FcmChecker
// -----------------------------------------------------------------------------
class FcmChecker {
    static const std::chrono::seconds kCheckInterval;

    push::fcm::Client& m_cli;
    std::shared_ptr<io_context> m_ioc;
    boost::fibers::condition_variable m_cond;
    boost::fibers::mutex m_mtx;
    std::thread m_thread;
    std::atomic_bool m_available;
    bool m_stop;

public:
    explicit FcmChecker(push::fcm::Client& cli)
        : m_cli(cli)
        , m_ioc(std::make_shared<io_context>())
        , m_thread(std::bind(&FcmChecker::run, this))
        , m_available(false)
        , m_stop(false)
    {
        m_ioc->post([this]() {
            boost::fibers::fiber(&FcmChecker::checkLoop, this).detach();
        });
    }

    ~FcmChecker()
    {
        m_ioc->post([this]() {
            boost::fibers::fiber(&FcmChecker::stopCheckLoop, this).detach();
        });
        m_thread.join();
    }

    bool isAvailable() const
    {
        return m_available;
    }

private:
    void run()
    {
        setCurrentThreadName("push.fcmcheck");
        try {
            fibers::asio::round_robin rr(m_ioc);
            rr.run();
        } catch (std::exception& e) {
            LOGE << "exception caught: " << e.what();
        }
        LOGD << "exiting fcm checker thread";
    }

    void checkLoop()
    {
        LOGD << "enter FCM connectivity check loop";
        std::unique_lock<boost::fibers::mutex> l(m_mtx);
        while (!m_stop) {
            LOGD << "start check FCM connectivity";
            bool isOk = m_cli.checkConnectivity(*m_ioc);
            if (!isOk) {
                LOGW << "FCM is currentlly not available";
            }
            m_available = isOk;
            m_cond.wait_for(l, kCheckInterval);
        }
        LOGD << "exit FCM connectivity check loop";
    }

    void stopCheckLoop()
    {
        {
            std::unique_lock<boost::fibers::mutex> l(m_mtx);
            m_stop = true;
        }
        m_cond.notify_all();
        m_ioc->stop();
    }
};

const std::chrono::seconds FcmChecker::kCheckInterval(60);

// -----------------------------------------------------------------------------
// Section: ServiceImpl
// -----------------------------------------------------------------------------
static const std::string kAppDataKey = "bcmdata";
static const std::string kNotificationTitle = "BCM";
static const std::string kNotificationText = "You receive a BCM message.";
static const std::string kCallingNotificationText = "Incoming BCM Call.";
static const int32_t kNotificationTTL = 86400;
static const int32_t kCallingNotificationTTL = 15;
static const int kDefaultInitialDelayMillis = 100;
static const double kDefaultMultiplier = 2.f;
static const int kDefaultMaxDelayMillis = 4000;
static const int kDefaultMaxRetries = 10;

static const std::string kMiActivity = "com.bcm.im.push.MiPushActivity";
static const std::string kMiActivityV2 = 
                            "com.bcm.messenger.push.MiPushActivity";

class ServiceImpl : public apns::QosMgr::INotificationSender {
    std::shared_ptr<AccountsManager> m_accountMgr;
    push::fcm::Client m_fcmClient;
    push::apns::Service m_apnsService;
    push::apns::QosMgr m_apnsQosMgr;
    push::umeng::Client m_umengClient;
    Backoff*    m_backoff;
    int32_t     m_maxDelayMillis;
    int32_t     m_maxRetries;
    bool        m_stop;
    FiberPool   m_fiberPool;

public:
    ServiceImpl(std::shared_ptr<AccountsManager> accountMgr, 
                const RedisConfig& redisCfg, const ApnsConfig& apns, 
                const FcmConfig& fcm, const UmengConfig& umeng, 
                int concurrency)
        : m_accountMgr(accountMgr)
        , m_apnsQosMgr(apns, redisCfg, *this)
        , m_backoff(nullptr)
        , m_maxDelayMillis(0)
        , m_maxRetries(0)
        , m_stop(false)
        , m_fiberPool(concurrency)
    {
        boost::ignore_unused(redisCfg);
        if (!this->apns().init(apns)) {
            Log::flush();
            throw std::invalid_argument("illegal apns init configuration");
        }
        this->fcm().apiKey(fcm.apiKey);
        this->umeng().appMasterSecret(umeng.appMasterSecret)
                     .appKey(umeng.appKey)
                     .appMasterSecretV2(umeng.appMasterSecretV2)
                     .appKeyV2(umeng.appKeyV2);
        this->exponentialBackoff(kDefaultInitialDelayMillis, 
                                 kDefaultMultiplier);
        this->uniformJitter();
        this->maxDelay(kDefaultMaxDelayMillis);
        this->maxRetries(kDefaultMaxRetries);
        this->m_fiberPool.run("push.service");

        LOGD << "create";
    }

    virtual ~ServiceImpl()
    {
        if (m_backoff != nullptr) {
            delete m_backoff;
        }
        LOGD << "destroy";
    }

    FiberPool& fiberPool()
    {
        return m_fiberPool;
    }

    void exponentialBackoff(int32_t initialDelayMillis, double multiplier)
    {
        m_backoff = static_cast<Backoff*>(new ExponentialDelayBackoff(
            initialDelayMillis, multiplier));
    }

    void uniformJitter()
    {
        if (m_backoff != nullptr) {
            m_backoff = new UniformRandomBackoff(m_backoff);
        }
    }

    void maxDelay(int32_t maxDelayMillis)
    {
        m_maxDelayMillis = maxDelayMillis;
    }

    void maxRetries(int32_t times)
    {
        m_maxRetries = times;
    }

    push::fcm::Client& fcm()
    {
        return m_fcmClient;
    }

    push::apns::Service& apns()
    {
        return m_apnsService;
    }

    push::umeng::Client& umeng()
    {
        return m_umengClient;
    }

    void sendNotificationWithRetry(io_context& ioc, const std::string& pushType,
                                   const Notification& noti, error_code& ec)
    {
        auto notiText = pushType + "-" +jsonable::toPrintable(noti);
        LOGD << "send notification: " << notiText;
        RetryContext context(m_maxDelayMillis, m_maxRetries);
        while (!m_stop) {
            if (!context.willRetry()) {
                break;
            }
            ec.clear();
            sendNotificationInternal(ioc, pushType, noti, ec);
            if (!ec) {
                // LOGD << "sent notification: " << notiText;
                return;
            } else if (ec.value() == boost::asio::error::operation_not_supported) {
                LOGW << "failed to send notification: " << notiText << ", error: " << ec.message();
                return;
            }
            context.increaseRetryCount();
            LOGW << "failed to send notification: " << notiText << ", error: " << ec.message()
                 << ", retry count :" << context.getRetryCount() << "/" << m_maxRetries;
            int32_t millis = m_backoff->delayMillis(context);
            boost::this_fiber::sleep_for(std::chrono::milliseconds(millis));
            context.addDelayMillis(millis);
        }
    }

    void sendNotificationInternal(io_context& ioc, const std::string& pushType,
                                  const Notification& noti, error_code& ec)
    {
        if (pushType == TYPE_SYSTEM_PUSH_APNS) {
            sendApnsNotification(ioc, noti, ec);
        } else if (pushType == TYPE_SYSTEM_PUSH_UMENG) {
            sendUmengNotifiaction(ioc, noti, ec);
        } else if (pushType == TYPE_SYSTEM_PUSH_FCM) {
            sendFcmNotification(ioc, noti, ec);
        } else {
            ec = boost::asio::error::operation_not_supported;
            LOGE << "type is not supported: " << pushType << ", info: " << jsonable::toPrintable(noti);
        }
    }

    void sendApnsNotification(io_context& ioc, const Notification& noti, error_code& ec)
    {
        boost::ignore_unused(ioc);

        std::string bundleId = apns().bundleId(noti.apnsType());
        if (bundleId.empty()) {
            ec = boost::asio::error::operation_not_supported;
            return;
        }
    
        auto pApnsNoti = std::make_unique<apns::SimpleNotification>();
        auto& apnsNoti = *pApnsNoti;
    
        apnsNoti.badge(noti.badge());
        
        if (noti.getClass() == push::Classes::CALLING || (!noti.isSupportApnsPush())) {
            
            if (noti.getClass() == push::Classes::CALLING) {
                apnsNoti.expiryTime(kCallingNotificationTTL);
            } else {
                apnsNoti.expiryTime(kNotificationTTL);
            }
            
            if (!noti.voipApnsId().empty()) {
                apnsNoti.apnId(noti.voipApnsId()).voip(true);
            } else {
                ec = boost::asio::error::operation_not_supported;
                return;
            }
            apnsNoti.data(noti.rawContent());
        } else {
            apnsNoti.expiryTime(kNotificationTTL);
    
            nlohmann::json j = noti.rawContent();
            Notification::clearApnsChat(j);
            apnsNoti.data(j);
            
            if (!noti.apnsId().empty()) {
                apnsNoti.apnId(noti.apnsId()).voip(false);
            } else {
                ec = boost::asio::error::operation_not_supported;
                return;
            }
    
            uint64_t  incrBadge = 0;
            if (!RedisDbManager::Instance()->incr(noti.targetAddress().getUid(), bcm::REDISDB_KEY_APNS_UID_BADGE_PREFIX + noti.targetAddress().getUid(), incrBadge)) {
                LOGE << "RedisDbManager push counter false, uid: " << noti.targetAddress().getUid();
            } else {
                apnsNoti.badge(incrBadge);
            }
        }
        
        apnsNoti.bundleId(bundleId);
        apnsNoti.collapseId(noti.targetAddress().getUid());

        apns::SendResult result = apns().send(noti.apnsType(), apnsNoti);
        ec = result.ec;
        if (ec) {
            return;
        } else if (result.statusCode != static_cast<int>(boost::beast::http::status::ok)) {
            LOGE << "failed to send apns notification to device: " << noti.targetAddress()
                 << ", statusCode: " << std::to_string(result.statusCode)
                 << ", error: " << result.error
                 << ", SimpleNotification info: " << apnsNoti.toString();
            if (result.isUnregistered()) {
                // LOG
            } else if (result.statusCode != static_cast<int>(boost::beast::http::status::bad_request)) {
                ec = boost::asio::error::try_again;
            }
        } else {
            if (pApnsNoti->isVoip()) {
                m_apnsQosMgr.scheduleResend(noti.targetAddress(),
                                            noti.apnsType(),
                                            std::move(pApnsNoti));
            }
        }
    }

    void sendUmengNotifiaction(io_context& ioc, const Notification& noti, error_code& ec)
    {
        if (noti.umengId().empty()) {
            ec = boost::asio::error::operation_not_supported;
            return;
        }

        umeng::Notification umengNoti;
        umengNoti
            .deviceToken(noti.umengId())
            .extra(kAppDataKey, noti.rawContent())
            .ticker(kNotificationTitle)
            .title(kNotificationTitle)
            .displayType(umeng::DisplayType::NOTIFICATION)
            .openApp()
            .productionMode(true)
            .miPush(true)
            .miActivity(kMiActivityV2)
            .appKey(umeng().appKeyV2());

        if (noti.getClass() == push::Classes::CALLING) {
            umengNoti.expiryTime(std::chrono::system_clock::now() +
                                 std::chrono::seconds(kCallingNotificationTTL));
            umengNoti.text(kCallingNotificationText);
        } else {
            umengNoti.expiryTime(std::chrono::system_clock::now() +
                                 std::chrono::seconds(kNotificationTTL));
            umengNoti.text(kNotificationText);
        }

        umeng::SendResult result = umeng().send(ioc, umengNoti, umeng::AppVer::V2);
        ec = result.ec;
        if (ec) {
            return;
        } else if (result.statusCode != boost::beast::http::status::ok) {
            LOGE << "failed to send umeng notification to device: " << noti.targetAddress()
                    << ", taskId: " << result.taskId << ", umengid: " << noti.umengId()
                    << ", error: " << result.errorCode << ", errorMsg: " << result.errorMsg
                    << ", msgId: " << result.msgId << ", http::status: " << std::to_string(static_cast<int>(result.statusCode))
                    << ", info: " << umengNoti.serialize();
            
            ec = boost::asio::error::try_again;
        } else if (result.ret == "FAIL") {
            LOGE << "statusCode is ok but still failed to send umeng to device: " << noti.targetAddress()
                 << ", taskId: " << result.taskId << ", umengid: " << noti.umengId()
                 << ", error: " << result.errorCode << ", errorMsg: " << result.errorMsg
                 << ", msgId: " << result.msgId << ", http::status: " << std::to_string(static_cast<int>(result.statusCode))
                 << ", info: " << umengNoti.serialize();
            ec = boost::asio::error::try_again;
        }
    }

    void sendFcmNotification(io_context& ioc, const Notification& noti, error_code& ec)
    {
        boost::ignore_unused(ioc);

        if (noti.fcmId().empty()) {
            ec = boost::asio::error::operation_not_supported;
            return;
        }
        fcm::Notification fcmNoti;
        fcmNoti.destination(noti.fcmId())
                .addDataPart(kAppDataKey, noti.content())
                .addDataPart("notification", "")
                .title(kNotificationTitle);

        if (noti.getClass() == push::Classes::CALLING) {
            fcmNoti.priority(fcm::Priority::HIGH);
            fcmNoti.ttl(kCallingNotificationTTL);
            fcmNoti.text(kCallingNotificationText);
        } else {
            fcmNoti.priority(fcm::Priority::NORMAL);
            fcmNoti.ttl(kNotificationTTL);
            fcmNoti.text(kNotificationText);
        }

        fcm::SendResult result = fcm().send(ioc, fcmNoti);
        ec = result.ec;
        if (ec) {
            return;
        } else if (result.isUnregistered()
                   || result.isInvalidRegistrationId()) {
            LOGW << "cannot send fcm notification to invalid device: " << noti.targetAddress()
                 << ", statusCode: " << std::to_string(static_cast<int>(result.statusCode))
                 << ", messageId: " << result.messageId
                 << ", error: " << result.error
                 << ", info: " << jsonable::toPrintable(noti);
        } else if (result.hasCanonicalRegistrationId()) {
            LOGW << "cannot send fcm notification to canonical device: " << noti.targetAddress()
                    << ", statusCode: " << std::to_string(static_cast<int>(result.statusCode))
                    << ", messageId: " << result.messageId
                    << ", error: " << result.error
                    << ", info: " << jsonable::toPrintable(noti);
        } else if (!result.isSuccess()) {
            LOGW << "device: " << noti.targetAddress() << ", meet unrecoverable error: " << result.error
                    << ", statusCode: " << std::to_string(static_cast<int>(result.statusCode))
                    << ", messageId: " << result.messageId
                    << ", error: " << result.error
                    << ", info: " << jsonable::toPrintable(noti);
        } else if (result.statusCode != boost::beast::http::status::ok) {
            LOGE << "failed to send fcm notification to device: " << noti.targetAddress()
                    << ", statusCode: " << std::to_string(static_cast<int>(result.statusCode))
                    << ", messageId: " << result.messageId
                    << ", error: " << result.error
                    << ", info: " << jsonable::toPrintable(noti);
        }
    }

    void broadcastFcmNotification(io_context& ioc, const std::string& topic,
                                            const Notification& noti, error_code& ec)
    {
        push::fcm::Notification notification;
        notification.topic(topic)
                .addDataPart(kAppDataKey, noti.content())
                .addDataPart("notification", "")
                .title(kNotificationTitle)
                .text(kNotificationText);

        push::fcm::SendResult result = fcm().send(ioc, notification, true);
        ec = result.ec;
        LOGD << "broadcast fcm notification to topic: " << topic << ", error: " << ec;
    }
    
    void broadcastUmengNotifiaction(io_context& ioc, const std::string& topic,
                               const Notification& noti, error_code& ec)
    {
        push::umeng::Notification notification;
        notification
                .filter(topic)
                .extra(kAppDataKey, noti.content())
                .ticker(kNotificationTitle)
                .title(kNotificationTitle)
                .text(kNotificationText)
                .displayType(push::umeng::DisplayType::NOTIFICATION)
                .openApp()
                .productionMode(true)
                .miPush(true)
                .miActivity(kMiActivityV2)
                .appKey(umeng().appKeyV2());

        push::umeng::SendResult result = umeng().send(ioc, notification, push::umeng::AppVer::V2);
        ec = result.ec;
        if (ec) {
            return;
        } else if (result.statusCode != boost::beast::http::status::ok) {
            LOGE << "failed to broadcast umeng notification to topic: " << topic
                 << ", error: " << result.errorCode << ", " << result.errorMsg;
            ec = boost::asio::error::try_again;
        } else if (result.ret.compare("FAIL") == 0) {
            LOGE << "statusCode is ok but still failed to broadcast umeng notification to topic: "<< topic
                 << ", error: " << result.errorCode << ", " << result.errorMsg;
            ec = boost::asio::error::try_again;
        }
    }

    void broadcastNotificationInternal(io_context& ioc,
                                       const std::string& topic,
                                       const Notification& noti,
                                       error_code& ec)
    {
        LOGI << "broadcast: " << jsonable::toPrintable(noti);
        broadcastUmengNotifiaction(ioc, topic, noti, ec);
        broadcastFcmNotification(ioc, topic, noti, ec);
    }

    void broadcastNotificationWithRetry(io_context& ioc,
                                        const std::string& topic,
                                        const Notification& noti,
                                        error_code& ec)
    {
        RetryContext context(m_maxDelayMillis, m_maxRetries);
        while (!m_stop) {
            if (!context.willRetry()) {
                break;
            }
            ec.clear();
            broadcastNotificationInternal(ioc, topic, noti, ec);
            if (!ec) {
                LOGD << "broadcast notification to topic: " << topic << " has been sent";
                return;
            }
            context.increaseRetryCount();
            LOGW << "failed to broadcast notification: " << jsonable::toPrintable(noti)
                 << ", error: " << ec.message()
                 << ", retry count: " << context.getRetryCount();
            int32_t millis = m_backoff->delayMillis(context);
            boost::this_fiber::sleep_for(std::chrono::milliseconds(millis));
            context.addDelayMillis(millis);
        }
    }

    // QosMgr::INotificationSender
    void sendNotification(const std::string& apnsType, 
                          const apns::Notification& notification) override
    {
        io_context& ioc = fiberPool().getIOContext();
        apns::Notification* n = notification.clone();
        static_cast<push::apns::SimpleNotification*>(n)->expiryTime(
            apns().expirySecs());
        FiberPool::post(ioc, [this, &ioc, apnsType, n]() {
            sendApnsNotificationWithRetry(apnsType, *n);
            delete n;
        });
    }

    void sendApnsNotificationWithRetry(const std::string& apnsType, 
                                       const apns::Notification& notification)
    {
        RetryContext context(m_maxDelayMillis, m_maxRetries);
        while (!m_stop) {
            if (!context.willRetry()) {
                break;
            }
            push::apns::SendResult res = apns().send(apnsType, notification);
            if (res.ec) {
                LOGE << "error sending apns notification "
                     << ", messageCode: " << res.ec.message()
                     << ", notification: " << notification.toString();
                if (res.ec.value() ==
                        boost::asio::error::operation_not_supported) {
                    return;
                }
            } else if (res.statusCode != 200) {
                LOGE << "failed to send apns notification '"
                        << ", statusCode: " << res.statusCode
                        << ", error: " << res.error
                        << ", notification: " << notification.toString();
            } else {
                LOGD << "apns notification has been sent "
                        << ", notification: " << notification.toString();
                return;
            }
            context.increaseRetryCount();
            LOGW << "re-send notification , count: " << context.getRetryCount() << "/" << m_maxRetries
                    << ", notification: " << notification.toString();
            
            int32_t millis = m_backoff->delayMillis(context);
            boost::this_fiber::sleep_for(std::chrono::milliseconds(millis));
            context.addDelayMillis(millis);
        }
    }
};

// -----------------------------------------------------------------------------
// Section: Service
// -----------------------------------------------------------------------------
Service::Service(std::shared_ptr<AccountsManager> accountMgr, 
                 const RedisConfig& redisCfg, const ApnsConfig& apns, 
                 const FcmConfig& fcm, const UmengConfig& umeng, 
                 int concurrency)
    : m_pImpl(new ServiceImpl(accountMgr, redisCfg, apns, fcm, umeng, 
                              concurrency))
    , m_impl(*m_pImpl) {}

Service::~Service()
{
    if (m_pImpl != nullptr) {
        delete m_pImpl;
    }
}

void Service::sendNotification(const std::string& pushType, const Notification& noti)
{
    if (pushType.empty() || noti.getClass() == push::Classes::SILENT) {
        return;
    }

    shared_ptr self = shared_from_this();
    io_context& ioc = m_impl.fiberPool().getIOContext();
    FiberPool::post(ioc, [self, &ioc, pushType, noti]() {
        error_code ec;
        self->m_impl.sendNotificationWithRetry(ioc, pushType, noti, ec);
    });
}

void Service::broadcastNotification(const std::string& topic, const Notification& noti)
{
    shared_ptr self = shared_from_this();
    io_context& ioc = m_impl.fiberPool().getIOContext();
    FiberPool::post(ioc, [self, &ioc, topic, noti]() {
        error_code ec;
        self->m_impl.broadcastNotificationWithRetry(ioc, topic, noti, ec);
    });
}

Service::shared_ptr Service::exponentialBackoff(int32_t initialDelayMillis,
                                               double multiplier)
{
    m_impl.exponentialBackoff(initialDelayMillis, multiplier);
    return shared_from_this();
}

Service::shared_ptr Service::uniformJitter()
{
    m_impl.uniformJitter();
    return shared_from_this();
}

Service::shared_ptr Service::maxDelay(int32_t maxDelayMillis)
{
    m_impl.maxDelay(maxDelayMillis);
    return shared_from_this();
}

Service::shared_ptr Service::maxRetries(int32_t times)
{
    m_impl.maxRetries(times);
    return shared_from_this();
}

} // namespace push
} // namespace bcm
