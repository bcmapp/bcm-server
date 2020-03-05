#pragma once

#include <fiber/fiber_timer.h>
#include <redis/async_conn.h>
#include <config/redis_config.h>
#include <config/service_config.h>

namespace bcm {

class IMServiceRegister 
    : public FiberTimer::Task
    , public redis::AsyncConn::ISubscriptionHandler {
public:
    static constexpr int kKeepAliveInterval = 10000;

    IMServiceRegister(const RedisConfig& redis, const ServiceConfig& service);
    ~IMServiceRegister();

    std::vector<std::string>& getRegisterKeys() { return m_registerKeys; }

private:
    // timer task interfaces
    void run() override;
    void cancel() override;
    int64_t lastExecTimeInMilli() override;

    // redis callbacks
    void onSubscribeConnected(int status);
    void onPublishConnected(int status);

    // redis::AsyncConn::ISubscriptionHandler
    void onSubscribe(const std::string& chan);
    void onUnsubscribe(const std::string& chan);
    void onMessage(const std::string& chan, const std::string& msg);
    void onError(int code);

private:
    std::vector<std::string> m_registerKeys;
    int64_t m_execTime{0};
    std::atomic_bool m_isSubscribed{false};
    std::atomic_bool m_canPublish{false};
    std::thread m_eventThread;
    struct event_base* m_eventBase;
    redis::AsyncConn m_subscriber;
    redis::AsyncConn m_publisher;
};

}
