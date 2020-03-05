#include "apns_qos_mgr.h"
#include "apns_service.h"
#include "apns_notification.h"
#include "config/bcm_config.h"
#include "dispatcher/dispatch_address.h"
#include "redis/async_conn.h"
#include "redis/reply.h"
#include "utils/log.h"
#include "utils/thread_utils.h"
#include "utils/libevent_utils.h"
#include "utils/sync_latch.h"

#include "proto/message/message_protocol.pb.h"

#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/thread.h>
#include <event2/util.h>
#include <hiredis/hiredis.h>
#include <boost/core/ignore_unused.hpp>

#include <memory>
#include <thread>

namespace bcm {
namespace push {
namespace apns {
class QosMgrImpl;
// -----------------------------------------------------------------------------
// Section: ResendTask
// -----------------------------------------------------------------------------
class ResendTask {
public:
    enum State {
        IDLE,
        SCHEDULED,
        CANCELED
    };

public:
    ResendTask(QosMgrImpl& qosMgr, const DispatchAddress& addr, 
               const std::string& apnsType, Notification* notification);
    ~ResendTask();

    bool schedule();
    void cancel();
    void updateData(const std::string& apnsType, Notification* notification);
    void resetCount();

private:
    static void handleTimeout(evutil_socket_t fd, short events, void* arg);
    void onTimeout();
    void onCheckUserOnline(bool isOnline);

private:
    struct event m_evtTimer;
    QosMgrImpl& m_qosMgr;
    DispatchAddress m_addr;
    std::string m_apnsType;
    std::unique_ptr<Notification> m_notification;
    int32_t m_resendCount;
    State m_state;
};

// -----------------------------------------------------------------------------
// Section: QosMgrImpl
// -----------------------------------------------------------------------------
class QosMgrImpl 
    : public bcm::redis::AsyncConn::ISubscriptionHandler {

    friend class ResendTask;

    QosMgr::INotificationSender& m_sender;
    int32_t m_resendDelayMilliSecs;
    int32_t m_maxResendCount;

    struct event_base* m_eb;
    std::unique_ptr<std::thread> m_thread;
    redis::AsyncConn m_conn;
    redis::AsyncConn m_connPub;

    std::unordered_map<std::string, std::unique_ptr<ResendTask>> m_addrTaskMap;

public:
    QosMgrImpl(const ApnsConfig& apnsCfg, const RedisConfig& redisCfg, 
               QosMgr::INotificationSender& sender)
        : m_sender(sender)
        , m_resendDelayMilliSecs(apnsCfg.resendDelayMilliSecs)
        , m_maxResendCount(apnsCfg.maxResendCount)
        , m_eb(event_base_new())
        , m_thread(std::make_unique<std::thread>(
            std::bind(&QosMgrImpl::eventLoop, this)))
        , m_conn(m_eb, redisCfg.ip, redisCfg.port, redisCfg.password)
        , m_connPub(m_eb, redisCfg.ip, redisCfg.port, redisCfg.password)
    {
        m_conn.setOnReconnectHandler(std::bind(&QosMgrImpl::onRedisReconnected, 
                                               this, std::placeholders::_1));
        m_conn.start(std::bind(&QosMgrImpl::onRedisConnected, this, 
                                std::placeholders::_1, true));
        m_connPub.start(std::bind(&QosMgrImpl::onRedisConnected, this,
                                    std::placeholders::_1, false));

        SyncLatch sl(2);
        bcm::libevent::AsyncFunc::invoke(m_eb, [&sl]() {
            sl.sync();
        });
        sl.sync();
        LOGD << "apns qos manager is running";
    }

    virtual ~QosMgrImpl()
    {
        m_conn.shutdown([](int res) {
            LOGD << "connection to redis for subscribe is shutdown: " << res;
        });
        m_connPub.shutdown([](int res) {
            LOGD << "connection to redis for publish is shutdown: " << res;
        });
        m_thread->join();
        event_base_free(m_eb);
    }

    void scheduleResend(const DispatchAddress& addr, 
                        const std::string& apnsType, 
                        std::unique_ptr<Notification> notification)
    {
        bcm::libevent::AsyncFunc::invoke(m_eb, 
            std::bind(&QosMgrImpl::doScheduleResend, this, addr, apnsType, 
                        notification.release() ) );
    }

private:
    void eventLoop()
    {
        LOGI << "anps qos manager thread started";
        setCurrentThreadName("push.apns.qos");
        int retVal = event_base_dispatch(m_eb);
        LOGI << "event_base_dispatch returned with value: " << retVal;
    }

    void doScheduleResend(const DispatchAddress& addr, 
                          const std::string& apnsType, 
                          Notification* notification)
    {
        const std::string& chan = addr.getSerializedForOnlineNotify();
        auto it = m_addrTaskMap.find(chan);
        if (it != m_addrTaskMap.end()) {
            LOGD << "resend already schedule for '" << addr.getSerialized() 
                 << "', update data";
            it->second->updateData(apnsType, notification);
            it->second->resetCount();
        } else {
            LOGD << "schedule resend for '" << addr.getSerialized() << "'";
            std::unique_ptr<ResendTask> task = std::make_unique<ResendTask>(
                *this, addr, apnsType, notification);
            if (task->schedule()) {
                m_addrTaskMap.emplace(chan, std::move(task));

                LOGD << "subscribe channel '" << chan << "'";
                m_conn.subscribe(chan, this);
            }
        }
    }

    void doUnscheduleResend(const DispatchAddress& addr)
    {
        const std::string& chan = addr.getSerializedForOnlineNotify();
        auto it = m_addrTaskMap.find(chan);
        if (it != m_addrTaskMap.end()) {
            LOGD << "unschedule resend for '" << addr.getSerialized() << "'";
            it->second->cancel();
            m_addrTaskMap.erase(it);
            m_conn.unsubscribe(chan);
        } else {
            LOGD << "resend for '" << addr.getSerialized() 
                 << "' has already been unscheduled";
        }
    }

    bool isTaskExists(const DispatchAddress& addr) const
    {
        return ( m_addrTaskMap.find(addr.getSerializedForOnlineNotify()) 
                    != m_addrTaskMap.end() );
    }

    // redis event callbacks
    void onRedisConnected(int status, bool isSubscribe)
    {
        if (REDIS_OK == status) {
            if (isSubscribe) {
                LOGD << "connection to redis for subscribe is established";
            } else {
                LOGD << "connection to redis for publish is established";
            }
        } else {
            LOGE << "error connecting redis: " << status;
        }
    }

    void onRedisReconnected(int status)
    {
        if (REDIS_OK == status) {
            LOGD << "redis reconnected, re-subscribe channels";
            for (auto it = m_addrTaskMap.begin(); it != m_addrTaskMap.end(); 
                    ++it) {
                m_conn.subscribe(it->first, this);
            }
        } else {
            LOGE << "error reconnecting redis: " << status;
        }
    }

    // AsyncConn::ISubscriptionHandler overrides
    void onSubscribe(const std::string& chan) override
    {
        LOGD << "redis channel '" << chan << "' subscribed";
    }
     
    void onUnsubscribe(const std::string& chan) override
    {
        LOGD << "redis channel '" << chan << "' unsubscribed";
    }

    void onMessage(const std::string& chan, const std::string& msg) override
    {
        bcm::PubSubMessage pubMsg;
        if (!pubMsg.ParseFromString(msg)) {
            LOGE << "failed to parse message '" << msg << "' from channel '" 
                 << chan << "'";
        } else if (pubMsg.type() != PubSubMessage::CONNECTED) {
            LOGE << "unexpected message type '" << pubMsg.type() 
                 << "' received from channel '" << chan << "'";
        } else {
            LOGD << "connect notify received from channel '" << chan << "'";
            auto it = m_addrTaskMap.find(chan);
            if (it != m_addrTaskMap.end()) {
                LOGD << "unschedule resend for '" << chan << "'";
                it->second->cancel();
                m_addrTaskMap.erase(it);
                m_conn.unsubscribe(chan);
            } else {
                LOGW << "no corresponding task for '" << chan << "'";
            }
        }
    }
    
    void onError(int code) override
    {
        LOGE << "redis error occured: " << code;
    }
};

// -----------------------------------------------------------------------------
// Section: ResendTask implementation
// -----------------------------------------------------------------------------
ResendTask::ResendTask(QosMgrImpl& qosMgr, const DispatchAddress& addr,
                       const std::string& apnsType, Notification* notification)
    : m_qosMgr(qosMgr), m_addr(addr), m_apnsType(apnsType)
    , m_notification(notification), m_resendCount(0), m_state(IDLE)
{
    event_assign(&m_evtTimer, m_qosMgr.m_eb, -1, EV_READ | EV_PERSIST, 
                 handleTimeout, (void*)this);
}

ResendTask::~ResendTask()
{
    LOGD << "resend task for '" << m_addr << "' destroyed";
    if (SCHEDULED == m_state) {
        cancel();
    }
}

bool ResendTask::schedule()
{
    struct timeval tv;
    evutil_timerclear(&tv);
    tv.tv_sec = m_qosMgr.m_resendDelayMilliSecs / 1000;
    tv.tv_usec = (m_qosMgr.m_resendDelayMilliSecs % 1000) * 1000;
    int retval = event_add(&m_evtTimer, &tv);
    if (retval != 0) {
        LOGE << "failed to set timer for '" << m_addr << "': " << retval;
        return false;
    }
    m_state = SCHEDULED;
    return true;
}

void ResendTask::cancel()
{
    event_del(&m_evtTimer);
    m_state = CANCELED;
}

void ResendTask::updateData(const std::string& apnsType, 
                            Notification* notification)
{
    m_apnsType = apnsType;
    m_notification.reset(notification);
}

void ResendTask::resetCount()
{
    m_resendCount = 0;
}

// static
void ResendTask::handleTimeout(evutil_socket_t fd, short events, void* arg)
{
    boost::ignore_unused(fd, events);
    ResendTask* pThis = reinterpret_cast<ResendTask*>(arg);
    pThis->onTimeout();
}

void ResendTask::onTimeout()
{
    LOGD << "resend task for '" << m_addr << "' is timeout";
    const std::string& chan = m_addr.getSerialized();
    
    bcm::PubSubMessage msg;
    std::string msgStr;
    msg.set_type(PubSubMessage::CHECK);
    msg.SerializeToString(&msgStr);

    // check if user is online
    m_qosMgr.m_connPub.exec([this](int res, const redis::Reply& reply) {
        LOGD << "done check if '" << m_addr << "' is online, res: " << res 
             << ", reply: " << reply;
        onCheckUserOnline( (REDIS_OK == res) && (reply.getInteger() == 1) );
    }, "PUBLISH %b %b", chan.c_str(), chan.size(), msgStr.c_str(), 
            msgStr.size());
}

void ResendTask::onCheckUserOnline(bool isOnline)
{
    if (!m_qosMgr.isTaskExists(m_addr)) {
        LOGI << "resend task for '" << m_addr.getSerialized() 
             << "' has already been unscheduled";
        return;
    }
    if (isOnline) {
        LOGI << "'" << m_addr.getSerialized() << "' is online, unschedule task";
        m_qosMgr.doUnscheduleResend(m_addr);
        return;
    }
    LOGI << "resend notification to '" << m_addr.getSerialized() 
         << "', apnsType: " << m_apnsType << ", token: " 
         << m_notification->token() << ", payload: " 
         << m_notification->payload() << ", expirtyTime: " 
         << m_notification->expiryTime() << ", topic: " 
         << m_notification->topic() << ", collapseId: " 
         << m_notification->collapseId() << " resendCount: " 
         << m_resendCount + 1 << "/" << m_qosMgr.m_maxResendCount;
    m_qosMgr.m_sender.sendNotification(m_apnsType, *m_notification);
    m_resendCount += 1;
    if (m_resendCount >= m_qosMgr.m_maxResendCount) {
        m_qosMgr.doUnscheduleResend(m_addr);
    }
}

// -----------------------------------------------------------------------------
// Section: QosMgr
// -----------------------------------------------------------------------------
QosMgr::QosMgr(const ApnsConfig& apnsCfg, const RedisConfig& redisCfg, 
               INotificationSender& sender)
    : m_pImpl(new QosMgrImpl(apnsCfg, redisCfg, sender))
    , m_impl(*m_pImpl)
{
}

QosMgr::~QosMgr()
{
    if (m_pImpl != nullptr) {
        delete m_pImpl;
    }
}

void QosMgr::scheduleResend(const DispatchAddress& addr, 
                            const std::string& apnsType, 
                            std::unique_ptr<Notification> notification)
{
    m_impl.scheduleResend(addr, apnsType, std::move(notification));
}

} // namespace apns
} // namespace push
} // namespace bcm