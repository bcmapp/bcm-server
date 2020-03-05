#pragma once

#include <shared_mutex>

#include <fiber/fiber_timer.h>
#include <redis/async_conn.h>
#include <config/redis_config.h>
#include <config/service_config.h>

namespace bcm {

class OfflineServiceRegister
    : public FiberTimer::Task
    , public redis::AsyncConn::ISubscriptionHandler {
public:
    static constexpr int kKeepAliveInterval = 5000;
    static constexpr int kReconnectDelay = 1000;

    explicit OfflineServiceRegister(const RedisConfig& redis);
    OfflineServiceRegister(const RedisConfig& redis,
                           const ServiceConfig& service,
                           const std::vector<std::string>& pushTypes);
    ~OfflineServiceRegister() final;

    std::string getRandomOfflineServerByType(const std::string& pushType)
    {
        if (pushType.empty()) {
            return "";
        }
        std::unique_lock<std::shared_timed_mutex> l(m_svrsMtx);
        if (m_pushOfflineSvrs.find(pushType) != m_pushOfflineSvrs.end()) {
            std::vector<std::string>& svrPush = m_pushOfflineSvrs[pushType];
            return svrPush[(m_iRoundRobin++) % svrPush.size()];
        }

        return "";
    }

private:
    // timer task interfaces
    void run() override;
    void cancel() override;
    int64_t lastExecTimeInMilli() override;

    // redis callbacks
    void onSubscribeConnected(int status);
    void onPublishConnected(int status);

    // redis::AsyncConn::ISubscriptionHandler
    void onSubscribe(const std::string& chan) override;
    void onUnsubscribe(const std::string& chan) override;
    void onMessage(const std::string& chan, const std::string& msg) override;
    void onError(int code) override;

    //
    void onPublishServer();
    void updateOfflineServerList();

private:
    std::vector<std::string> m_registerKeys;

    int64_t m_execTime{0};
    bool m_getServerListOnly{false};
    std::atomic_bool m_canPublish{false};
    
    std::thread m_eventThread;
    struct event_base* m_eventBase;
    std::unique_ptr<redis::AsyncConn> m_subscriber{nullptr};
    std::unique_ptr<redis::AsyncConn> m_publisher{nullptr};
    std::string m_pushType;
    
    mutable std::shared_timed_mutex m_svrsMtx;
    std::map<std::string /* pushType */, std::vector<std::string> >  m_pushOfflineSvrs;
    std::set<std::string>  m_offlineSrvs;
    uint64_t m_iRoundRobin{0};
};

}
