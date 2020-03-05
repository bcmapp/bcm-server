#include "imservice_register.h"
#include <event.h>
#include <hiredis/read.h>
#include <redis/reply.h>
#include <utils/log.h>
#include <utils/thread_utils.h>
#include <utils/time.h>

namespace bcm {

static constexpr int kReconnectDelay = 3000;
static constexpr char kKeepAliveMessage[] = "keep alive";
static constexpr char kChannelPrefix[] = "imserver_";

IMServiceRegister::IMServiceRegister(const RedisConfig& redis, const ServiceConfig& service)
    : m_eventBase(event_base_new())
    , m_subscriber(m_eventBase, redis.ip, static_cast<int16_t>(redis.port), redis.password)
    , m_publisher(m_eventBase, redis.ip, static_cast<int16_t>(redis.port), redis.password)
{
    for (auto& ip : service.ips) {
        std::string key(kChannelPrefix);
        m_registerKeys.push_back(key + ip + ":" + std::to_string(service.port));
    }

    if (!m_registerKeys.empty()) {
        m_subscriber.setReconnectDelay(kReconnectDelay);
        m_subscriber.setOnReconnectHandler(std::bind(&IMServiceRegister::onSubscribeConnected, this,
                                                     std::placeholders::_1));
        m_subscriber.start(std::bind(&IMServiceRegister::onSubscribeConnected, this, std::placeholders::_1));

        m_publisher.setOnReconnectHandler(std::bind(&IMServiceRegister::onPublishConnected, this,
                                                    std::placeholders::_1));
        m_publisher.start(std::bind(&IMServiceRegister::onPublishConnected, this, std::placeholders::_1));

        m_eventThread = std::thread([&] {
            setCurrentThreadName("register.imsrv");
            int ret = event_base_dispatch(m_eventBase);
            LOGI << "event loop finish: " << ret;
        });
    }
}

IMServiceRegister::~IMServiceRegister()
{
    m_subscriber.shutdown([](int){});
    m_publisher.shutdown([](int){});

    event_base_loopbreak(m_eventBase);
    m_eventThread.join();

    event_base_free(m_eventBase);
    m_eventBase = nullptr;
}

void IMServiceRegister::run()
{
    if (!m_canPublish) {
        return;
    }

    int start = nowInMilli();

    for (auto& key : m_registerKeys) {
        std::shared_ptr<fibers::promise<int>> promise = std::make_shared<fibers::promise<int>>();
        m_publisher.exec([promise](int status, const redis::Reply& reply) {
            if (status != REDIS_OK) {
                promise->set_value(-1);
                return;
            }

            if (!reply.isInteger()) {
                promise->set_value(-2);
                return;
            }
            promise->set_value(reply.getInteger());
        }, "PUBLISH %b %b", key.data(), key.size(), kKeepAliveMessage, sizeof(kKeepAliveMessage) - 1);

        int ret = promise->get_future().get();
        if (ret <= 0) {
            LOGW << "publish to " << key << "failed: " << ret;
        }
    }
    m_execTime = nowInMilli() - start;
}

void IMServiceRegister::cancel()
{
    LOGI << "cancel keep alive";
}

int64_t IMServiceRegister::lastExecTimeInMilli()
{
    return m_execTime;
}

void IMServiceRegister::onSubscribeConnected(int status)
{
    if (status != REDIS_OK) {
        LOGW << "subscriber connect faild: " << status;
        return;
    }

    LOGI << "subscriber connected";

    for (auto& key : m_registerKeys) {
        m_subscriber.subscribe(key, this);
    }
}

void IMServiceRegister::onSubscribe(const std::string& chan)
{
    LOGT << "subscribed to channel " << chan;
}

void IMServiceRegister::onUnsubscribe(const std::string& chan)
{
    LOGT << "unsubscribed from channel " << chan;
}

void IMServiceRegister::onMessage(const std::string& chan, 
                                  const std::string& msg)
{
    static uint64_t times = 0;
    // print every 2 minutes
    int timesPer5Minutes = ((2 * 60 * 1000) / kKeepAliveInterval) - 1;
    if ((times % timesPer5Minutes) == 0) {
        LOGI << "received message: " << msg << ", on: " << chan << ", times: " << times;
    }
    ++times;
}                                  

void IMServiceRegister::onError(int code)
{
    LOGE << "redis error: " << code;
}

void IMServiceRegister::onPublishConnected(int status)
{
    if (status != REDIS_OK) {
        m_canPublish = false;
        LOGW << "publisher connect faild: " << status;
        return;
    }

    LOGI << "publisher connected";

    m_canPublish = true;
}

}