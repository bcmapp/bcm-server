#include "offline_register.h"
#include <event.h>
#include <hiredis/read.h>
#include <redis/reply.h>
#include <utils/log.h>
#include <utils/thread_utils.h>
#include <utils/time.h>

namespace bcm {

static constexpr char kKeepAliveMessage[] = "keep alive";
static constexpr char kChannelPrefix[] = "offlinesvr_";

OfflineServiceRegister::OfflineServiceRegister(const bcm::RedisConfig& redis)
    : m_getServerListOnly(true)
    , m_eventBase(event_base_new())
{
    m_publisher = std::make_unique<redis::AsyncConn>(m_eventBase, redis.ip,
                                                     static_cast<int16_t>(redis.port), redis.password);
    m_publisher->setReconnectDelay(OfflineServiceRegister::kReconnectDelay);
    m_publisher->setOnReconnectHandler(std::bind(&OfflineServiceRegister::onPublishConnected, this,
                                                 std::placeholders::_1));
    m_publisher->start(std::bind(&OfflineServiceRegister::onPublishConnected, this, std::placeholders::_1));

    m_eventThread = std::thread([&] {
        setCurrentThreadName("reg.offlineSvr");
        int ret = event_base_dispatch(m_eventBase);
        LOGI << "event loop finish: " << ret;
    });
}

OfflineServiceRegister::OfflineServiceRegister(const RedisConfig& redis,
                                               const ServiceConfig& service,
                                               const std::vector<std::string>& pushTypes)
    : m_getServerListOnly(false)
    , m_eventBase(event_base_new())
{
    uint32_t typeIndex = 0;
    for (const auto& type : pushTypes) {
        if (0 != typeIndex) {
            m_pushType += ",";
        }
        m_pushType += type;
        typeIndex++;
    }

    for (const auto& ip : service.ips) {
        m_registerKeys.push_back(kChannelPrefix + ip + ":" + std::to_string(service.port) + "_" + m_pushType);
        break;
    }

    if (!m_registerKeys.empty()) {
        m_subscriber = std::make_unique<redis::AsyncConn>(m_eventBase, redis.ip,
                                                          static_cast<int16_t>(redis.port), redis.password);
        m_subscriber->setReconnectDelay(OfflineServiceRegister::kReconnectDelay);
        m_subscriber->setOnReconnectHandler(std::bind(&OfflineServiceRegister::onSubscribeConnected, this,
                                                     std::placeholders::_1));
        m_subscriber->start(std::bind(&OfflineServiceRegister::onSubscribeConnected, this, std::placeholders::_1));

        m_publisher = std::make_unique<redis::AsyncConn>(m_eventBase, redis.ip,
                                                         static_cast<int16_t>(redis.port), redis.password);
        m_publisher->setReconnectDelay(OfflineServiceRegister::kReconnectDelay);
        m_publisher->setOnReconnectHandler(std::bind(&OfflineServiceRegister::onPublishConnected, this,
                                                    std::placeholders::_1));
        m_publisher->start(std::bind(&OfflineServiceRegister::onPublishConnected, this, std::placeholders::_1));

        m_eventThread = std::thread([&] {
            setCurrentThreadName("reg.offlineSvr");
            int ret = event_base_dispatch(m_eventBase);
            LOGI << "event loop finish: " << ret;
        });
    }
}

OfflineServiceRegister::~OfflineServiceRegister()
{
    if (m_subscriber) {
        m_subscriber->shutdown([](int){});
    }
    if (m_publisher) {
        m_publisher->shutdown([](int){});
    }

    event_base_loopbreak(m_eventBase);
    m_eventThread.join();

    event_base_free(m_eventBase);
    m_eventBase = nullptr;
}

void OfflineServiceRegister::onPublishServer()
{
    if (!m_canPublish || m_getServerListOnly) {
        return;
    }

    for (auto& key : m_registerKeys) {
        std::shared_ptr<fibers::promise<int>> promise = std::make_shared<fibers::promise<int>>();
        m_publisher->exec([promise](int status, const redis::Reply& reply) {
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
            LOGW << "publish to key: " << key << "failed: " << ret;
        }
    }
}

void OfflineServiceRegister::updateOfflineServerList()
{
    LOGD << "start update OfflineServiceRegister list";

    std::string sRedisCmd(kChannelPrefix);
    sRedisCmd = "PUBSUB CHANNELS " + sRedisCmd + "*";

    m_publisher->exec([this](int res, const redis::Reply& reply) {
        if (REDIS_OK != res) {
            LOGE << "send 'PUBSUB CHANNELS " << kChannelPrefix << "*' command error: " << res;
            return;
        }
        
        if (reply.isError()) {
            LOGE << "get OfflineServiceRegister list from channel: " << kChannelPrefix
                    << ", redis error: " << reply.getError();
            return;
        }
        
        if (!reply.isArray()) {
            LOGE << "get OfflineService list from redis error, channel: " << kChannelPrefix
                    << ", unexpected reply type received: " << reply.type();
            return;
        }
        
        std::vector<std::string> serverDescList = reply.getStringList();
        if (serverDescList.empty()) {
            LOGI << "offline server list is empty";
            return;
        }

        bool shouldLog = ((rand() % 10) == 0);

        std::map<std::string /* pushType */, std::vector<std::string> >  tmpPushOfflineSvrs;
        std::set<std::string>  tmpOfflineSrvs;

        for (auto& ipPortEnt : serverDescList) {
            std::vector<std::string> tokens;
            boost::split(tokens, ipPortEnt, boost::is_any_of("_"));
            if (tokens.size() >= 3) {
                std::string& ipport = tokens[1];
                std::vector<std::string> types;
                boost::split(types, tokens[2], boost::is_any_of(","));

                if (tmpOfflineSrvs.find(ipport) == tmpOfflineSrvs.end()) {
                    tmpOfflineSrvs.insert(ipport);

                    for (const auto& it : types) {
                        tmpPushOfflineSvrs[it].emplace_back(ipport);
                    }
                }
            } else {
                LOGE << "invalid server descriptor format: " << ipPortEnt;
            }
        }

        {
            std::unique_lock<std::shared_timed_mutex> l(m_svrsMtx);
            if (m_offlineSrvs.size() != tmpOfflineSrvs.size()) {
                shouldLog = true;
            }

            std::swap(m_offlineSrvs, tmpOfflineSrvs);
            std::swap(m_pushOfflineSvrs, tmpPushOfflineSvrs);
        }

        if (shouldLog) {
            LOGI << "found " << " offline servers: " << toString(serverDescList);
        }

    }, sRedisCmd.c_str());
}


void OfflineServiceRegister::run()
{
    int start = nowInMilli();

    onPublishServer();
    updateOfflineServerList();

    m_execTime = nowInMilli() - start;
}

void OfflineServiceRegister::cancel()
{
    LOGI << "cancel keep alive";
}

int64_t OfflineServiceRegister::lastExecTimeInMilli()
{
    return m_execTime;
}

void OfflineServiceRegister::onSubscribeConnected(int status)
{
    if (status != REDIS_OK) {
        LOGW << "subscriber connect faild: " << status;
        return;
    }

    LOGI << "subscriber connected";

    for (auto& key : m_registerKeys) {
        m_subscriber->subscribe(key, this);
    }
}

void OfflineServiceRegister::onSubscribe(const std::string& chan)
{
    LOGT << "subscribed to channel " << chan;
}

void OfflineServiceRegister::onUnsubscribe(const std::string& chan)
{
    LOGT << "unsubscribed from channel " << chan;
}

void OfflineServiceRegister::onMessage(const std::string& chan,
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

void OfflineServiceRegister::onError(int code)
{
    LOGE << "redis error: " << code;
}

void OfflineServiceRegister::onPublishConnected(int status)
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