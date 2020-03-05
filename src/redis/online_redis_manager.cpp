#include "redis/online_redis_manager.h"
#include <event2/event.h>
#include "redis/reply.h"
#include <hiredis/hiredis.h>
#include <list>
#include "utils/thread_utils.h"
#include "utils/libevent_utils.h"
#include "metrics_client.h"

namespace bcm {

class RedisPartition {
    struct RedisAsyncConn {
        public:
        AsyncConn asyncConn;
        uint32_t priority;
        RedisAsyncConn(struct event_base* eb, const std::string& host, 
              int32_t port, const std::string& password, uint32_t pri)
              : asyncConn(eb, host, port, password)
              , priority(pri) {}
    };

    typedef std::unordered_map<std::string, AsyncConn::ISubscriptionHandler*> ChanHandlerMap;
    typedef std::map<uint32_t, std::shared_ptr<RedisAsyncConn>> RedisAsyncConnMap;

    //static constexpr int32_t kPublishMaxtTryTimes = 2;

    RedisAsyncConnMap m_allSubConnections;
    RedisAsyncConnMap m_availableSubConns;
    std::shared_timed_mutex m_subConnMutex;

    RedisAsyncConnMap m_allPubConnections;
    RedisAsyncConnMap m_availablePubConns;
    std::shared_timed_mutex m_pubConnMutex;

    ChanHandlerMap m_subChansMap;
    ChanHandlerMap m_psubChansMap;
    std::shared_timed_mutex m_chanMutex;

    std::string m_partitionName;

public:
    RedisPartition(struct event_base* eb, const std::vector<RedisConfig>& partitionRedis,
                   std::string partitionName)
                   : m_partitionName(partitionName)
    {
        if (partitionRedis.empty()) {
            return;
        }

        for (uint32_t i = 0; i < partitionRedis.size(); ++i) {
            prepareSubConnection(eb, partitionRedis[i], i);
            preparePubConnection(eb, partitionRedis[i], i);
        }
    }

    ~RedisPartition()
    {
        for (auto& c : m_allSubConnections) {
            c.second->asyncConn.shutdown([this, &c](int status) {
                LOGI << "online redis manager partition '" << m_partitionName << "' for sub, ip '" << c.second->asyncConn.getRedisHost() 
                     << ", port '" << c.second->asyncConn.getRedisPort() << "' is shutdown, status: " << status;
            });
        }

        for (auto& c : m_allPubConnections) {
            c.second->asyncConn.shutdown([this, &c](int status) {
                LOGI << "online redis manager partition '" << m_partitionName << "' for pub, ip '" << c.second->asyncConn.getRedisHost() 
                     << ", port '" << c.second->asyncConn.getRedisPort() << "' is shutdown, status: " << status;
            });
        }

    }

    bool unsubscribe(const std::string& chan)
    {
        if (chan == kKeepAliveChannel) {
            return true;
        }

        {
            std::unique_lock<std::shared_timed_mutex> l(m_chanMutex);
            auto itr = m_subChansMap.find(chan);
            if (itr != m_subChansMap.end()) {
                m_subChansMap.erase(itr);
            }
        }

        {
            std::shared_lock<std::shared_timed_mutex> l(m_subConnMutex);

            if (m_availableSubConns.empty()) {
                LOGE << "online redis manager partition '" << m_partitionName 
                     << "' no available redis for unsubscribe: " << chan;
                metrics::MetricsClient::Instance()->markMicrosecondAndRetCode(kOnlineRedisServiceName,
                                                                              kOnlineRedisTopicName,
                                                                              0,
                                                                              10001);
                return false;
            }

            for (auto& c : m_availableSubConns) {
                c.second->asyncConn.unsubscribe(chan);
            }
        }

        return true;
    }

    bool punsubscribe(const std::string& chan)
    {
        {
            std::unique_lock<std::shared_timed_mutex> l(m_chanMutex);
            auto itr = m_psubChansMap.find(chan);
            if (itr != m_psubChansMap.end()) {
                m_psubChansMap.erase(itr);
            }
        }

        {
            std::shared_lock<std::shared_timed_mutex> l(m_subConnMutex);

            if (m_availableSubConns.empty()) {
                LOGE << "online redis manager partition '" << m_partitionName 
                     << "' no available redis for punsubscribe: " << chan;
                metrics::MetricsClient::Instance()->markMicrosecondAndRetCode(kOnlineRedisServiceName,
                                                                              kOnlineRedisTopicName,
                                                                              0,
                                                                              10001);
                return false;
            }

            for (auto& c : m_availableSubConns) {
                c.second->asyncConn.punsubscribe(chan);
            }
        }

        return true;
    }

    bool subscribe(const std::string& chan, AsyncConn::ISubscriptionHandler* handler)
    {
        {
            std::unique_lock<std::shared_timed_mutex> l(m_chanMutex);
            m_subChansMap[chan] = handler;
        }
        
        {
            std::shared_lock<std::shared_timed_mutex> l(m_subConnMutex);

            if (m_availableSubConns.empty()) {
                LOGE << "online redis manager partition '" << m_partitionName 
                     << "' no available redis for subscribe: " << chan;
                metrics::MetricsClient::Instance()->markMicrosecondAndRetCode(kOnlineRedisServiceName,
                                                                              kOnlineRedisTopicName,
                                                                              0,
                                                                              10001);
                return false;
            }

            for (auto& c : m_availableSubConns) {
                c.second->asyncConn.subscribe(chan, handler);
            }
        }

        return true;
    }

    bool psubscribe(const std::string& chan, AsyncConn::ISubscriptionHandler* handler)
    {
        {
            std::unique_lock<std::shared_timed_mutex> l(m_chanMutex);
            m_psubChansMap[chan] = handler;
        }

        {
            std::shared_lock<std::shared_timed_mutex> l(m_subConnMutex);

            if (m_availableSubConns.empty()) {
                LOGE << "online redis manager partition '" << m_partitionName 
                     << "' no available redis for psubscribe: " << chan;
                metrics::MetricsClient::Instance()->markMicrosecondAndRetCode(kOnlineRedisServiceName,
                                                                              kOnlineRedisTopicName,
                                                                              0,
                                                                              10001);
                return false;
            }

            for (auto& c : m_availableSubConns) {
                c.second->asyncConn.psubscribe(chan, handler);
            }
        }

        return true;
    }

    bool publish(const std::string& chan, const std::string& msg,
                 AsyncConn::ReplyHandler&& handler)
    {
        return tryPublish(m_availablePubConns, chan, msg, std::forward<AsyncConn::ReplyHandler>(handler));
    }

    bool isSubscribed(const std::string& chan)
    {
        std::shared_lock<std::shared_timed_mutex> l(m_chanMutex);
        return m_subChansMap.find(chan) != m_subChansMap.end();
    }

private:
    void prepareSubConnection(struct event_base* eb, const RedisConfig& redisCfg, uint32_t priority)
    {
        auto conn = std::make_shared<RedisAsyncConn>(eb, redisCfg.ip, redisCfg.port, redisCfg.password, priority);
        
        conn->asyncConn.setOnDisconnectHandler([this, conn](int status) {
            boost::ignore_unused(status);
            {
                // Remove conn from available subscribe connectios
                std::unique_lock<std::shared_timed_mutex> l(m_subConnMutex);
                m_availableSubConns.erase(conn->priority);
            }
            LOGW << "online redis manager for sub disconnect, partition: " << m_partitionName 
                 << ", ip: " << conn->asyncConn.getRedisHost()
                 << ", port: " << conn->asyncConn.getRedisPort();
        });

        // Already re-connected handler
        conn->asyncConn.setOnReconnectHandler([this, conn](int status) {
            if (REDIS_OK != status) {
                return;
            }

            onSubConnect(conn);

            LOGI << "online redis manager for sub reconnect, partition: " << m_partitionName
                 << ", ip: " << conn->asyncConn.getRedisHost()
                 << ", port: " << conn->asyncConn.getRedisPort();
        });

        conn->asyncConn.start([this, conn](int status) {
            if (REDIS_OK != status) {
                return;
            }

            onSubConnect(conn);

            LOGI << "online redis manager for sub connect success, partition: " << m_partitionName
                 << ", ip: " << conn->asyncConn.getRedisHost()
                 << ", port: " << conn->asyncConn.getRedisPort();
        });

        m_allSubConnections[conn->priority] = conn;
    }

    void preparePubConnection(struct event_base* eb, const RedisConfig& redisCfg, uint32_t priority)
    {
        auto conn = std::make_shared<RedisAsyncConn>(eb, redisCfg.ip, redisCfg.port, redisCfg.password, priority);
        
        conn->asyncConn.setOnDisconnectHandler([this, conn](int status) {
            boost::ignore_unused(status);
            {
                std::unique_lock<std::shared_timed_mutex> l(m_pubConnMutex);
                m_availablePubConns.erase(conn->priority);
            }  
            LOGW << "online redis manager for pub disconnect, partition: " << m_partitionName 
                 << ", ip: " << conn->asyncConn.getRedisHost()
                 << ", port: " << conn->asyncConn.getRedisPort();       
        });

        conn->asyncConn.setOnReconnectHandler([this, conn](int status) {
            if (REDIS_OK !=  status) {
                return;
            }

            {
                std::unique_lock<std::shared_timed_mutex> l(m_pubConnMutex);
                m_availablePubConns[conn->priority] = conn;
            }

            LOGI << "online redis manager for pub reconnect, partition: " << m_partitionName
                 << ", ip: " << conn->asyncConn.getRedisHost()
                 << ", port: " << conn->asyncConn.getRedisPort();
        });

        conn->asyncConn.start([this, conn](int status) {
            if (REDIS_OK != status) {
                return;
            }

            {
                std::unique_lock<std::shared_timed_mutex> l(m_pubConnMutex);
                m_availablePubConns[conn->priority] = conn;
            }

            LOGI << "online redis manager for pub connect success, partition: " << m_partitionName
                 << ", ip: " << conn->asyncConn.getRedisHost()
                 << ", port: " << conn->asyncConn.getRedisPort();   
        });

        m_allPubConnections[conn->priority] = conn;
    }

    void onSubConnect(std::shared_ptr<RedisAsyncConn> conn)
    {
        {
            std::unique_lock<std::shared_timed_mutex> l(m_subConnMutex);
            m_availableSubConns[conn->priority] = conn;
        }

        ChanHandlerMap subChanTmp, psubChanTmp;                
        {
            std::shared_lock<std::shared_timed_mutex> l(m_chanMutex);
            subChanTmp = m_subChansMap;
            psubChanTmp = m_psubChansMap;
        }

        for (auto& chan : subChanTmp) {
            conn->asyncConn.subscribe(chan.first, chan.second);
        }

        for (auto &chan : psubChanTmp) {
            conn->asyncConn.psubscribe(chan.first, chan.second);
        }
    }

    bool tryPublish(RedisAsyncConnMap& connMap, const std::string& chan,
                    const std::string& msg, AsyncConn::ReplyHandler&& handler)
    {
        std::shared_ptr<RedisAsyncConn> conn = nullptr;

        {
            // Try the 1st connection first
            std::shared_lock<std::shared_timed_mutex> l(m_pubConnMutex);
            conn = connMap.empty() ? nullptr : connMap.begin()->second;            
        }

        // No available connection exist
        if (nullptr == conn) {
            LOGE << "partition '" << m_partitionName << "' no available redis for publish: " << chan
                << ", message: " << msg;
            handler(REDIS_ERR, Reply());
            metrics::MetricsClient::Instance()->markMicrosecondAndRetCode(kOnlineRedisServiceName,
                                                                          kOnlineRedisTopicName,
                                                                          0,
                                                                          10001);
            return false;
        }

        conn->asyncConn.exec([this, chan, msg, h(std::forward<AsyncConn::ReplyHandler>(handler))]
            (int status, const Reply& reply) mutable {
            if (REDIS_OK != status) {
                LOGE << "partition '" << m_partitionName << "' publish fail, channel: " << chan << ", msg: " << msg;
                return;
            }

            if (!reply.isInteger()) {
                LOGE << "partition '" << m_partitionName << "' publish error, channel: " << chan << ", msg: " << msg
                    << ", reply: " << reply;
            }

            if (0 == reply.getInteger()) {
                LOGT << "partition '" << m_partitionName << "' no subscriber for channel: " << chan;
            }

            if (nullptr != h) {
                h(status, reply);
            }
        }, "PUBLISH %b %b", chan.data(), chan.size(), msg.data(), msg.size());

        return true;
    }
};

OnlineRedisManager::OnlineRedisManager()
            : m_eb(event_base_new())
{
}

OnlineRedisManager::~OnlineRedisManager()
{
    event_del(&m_evtKeepAlive);
    libevent::AsyncFunc::invoke(m_eb, [this]() {
        event_base_loopbreak(m_eb);
    });
    m_thread.join();
    event_base_free(m_eb);
    m_eb = nullptr;
}

bool OnlineRedisManager::init(const Partition2RedisCfgMap& redisConfig)
{
    if (redisConfig.empty()) {
        LOGE << "online redis manager config is empty";
        return false;
    }

    for (auto& pcfg : redisConfig) {
        if (pcfg.second.empty()) {
            LOGE << "online redis manager config of partition '" << pcfg.first << "' is empty";
            return false;
        }

        auto partition = std::make_shared<RedisPartition>(m_eb, pcfg.second, pcfg.first);
        m_partitions[pcfg.first] = partition;
        m_consistentHash.AddServer(pcfg.first);
    }

    return true;
}

void OnlineRedisManager::start()
{
    m_thread = std::thread([&]() {
        setCurrentThreadName("online.redis.m");
        LOGI << "online redis manager event loop start";

        try {
            event_assign(&m_evtKeepAlive, m_eb, -1, EV_READ | EV_PERSIST, onKeepAlive, (void*)this);
            struct timeval tv;
            evutil_timerclear(&tv);
            tv.tv_sec = kKeepAliveInterval;
            event_add(&m_evtKeepAlive, &tv);
            event_base_dispatch(m_eb);
        } catch(std::exception& e) {
            LOGE << "online redis manager start catch exception: " << e.what();
        }
    });
}

void OnlineRedisManager::runKeepAlive()
{
    for (auto& p : m_partitions) {
        p.second->unsubscribe(kKeepAliveChannel);
    }
}

std::shared_ptr<RedisPartition> OnlineRedisManager::getPartitionByHashKey(const std::string hashKey)
{
    std::string partitionName = m_consistentHash.GetServer(hashKey);
    if ("" == partitionName) {
        return nullptr;
    }

    auto itr = m_partitions.find(partitionName);
    if (itr == m_partitions.end()) {
        return nullptr;
    }

    return itr->second;
}

bool OnlineRedisManager::subscribe(const std::string& hashKey, const std::string& chan,
                                   AsyncConn::ISubscriptionHandler* handler)
{
    auto partition = getPartitionByHashKey(hashKey);
    if (nullptr == partition) {
        LOGE << "online redis manager no partition for hashKey: " << hashKey
             << ", subscribe: " << chan;
        return false;
    }

    return partition->subscribe(chan, handler);
}

bool OnlineRedisManager::subscribe(const std::string& chan, AsyncConn::ISubscriptionHandler* handler)
{
    return subscribe(chan, chan, handler);
}

bool OnlineRedisManager::psubscribe(const std::string& hashKey, const std::string& chan, 
                                    AsyncConn::ISubscriptionHandler* handler)
{
    auto partition = getPartitionByHashKey(hashKey);
    if (nullptr == partition) {
        LOGE << "online redis manager no partition for hashKey: " << hashKey
             << ", psubscribe: " << chan;
        return false;
    }

    return partition->psubscribe(chan, handler);
}

bool OnlineRedisManager::psubscribe(const std::string& chan, AsyncConn::ISubscriptionHandler* handler)
{
    return psubscribe(chan, chan, handler);
}

bool OnlineRedisManager::unsubscribe(const std::string& hashKey, const std::string& chan)
{
    auto partition = getPartitionByHashKey(hashKey);
    if (nullptr == partition) {
        LOGE << "online redis manager no partition for unsubscribe, hashKey: " << hashKey
             << ", channel: " << chan;
        return false;
    }

    return partition->unsubscribe(chan);
}

bool OnlineRedisManager::unsubscribe(const std::string& chan)
{
    return unsubscribe(chan, chan);
}

bool OnlineRedisManager::punsubscribe(const std::string& chan)
{
    auto partition = getPartitionByHashKey(chan);
    if (nullptr == partition) {
        LOGE << "online redis manager no partition for punsubscribe, hashKey: " << chan
             << ", channel: " << chan;
        return false;
    }

    return partition->punsubscribe(chan);
}

bool OnlineRedisManager::publish(const std::string& hashKey, const std::string& channel, 
                                 const std::string& message, AsyncConn::ReplyHandler&& handler)
{
    auto partition = getPartitionByHashKey(hashKey);
    if (nullptr == partition) {
        LOGE << "online redis manager no partition for publish, channel: " << channel
             << ", msg: " << message << ", hashKey: " << hashKey;
        return false;
    }

    return partition->publish(channel, message, std::forward<AsyncConn::ReplyHandler>(handler));
}

bool OnlineRedisManager::publish(const std::string& channel, const std::string& message, 
                                 AsyncConn::ReplyHandler&& handler)
{
    return publish(channel, channel, message, std::forward<AsyncConn::ReplyHandler>(handler));
}

bool OnlineRedisManager::isSubscribed(const std::string& chan)
{
    return isSubscribed(chan, chan);
}

bool OnlineRedisManager::isSubscribed(const std::string& hashKey, const std::string& chan)
{
    auto partition = getPartitionByHashKey(hashKey);
    if (nullptr == partition) {
        LOGE << "online redis manager no partition for channel: " << chan << ", hashKey: " << hashKey;
        return false;
    }
    return partition->isSubscribed(chan);
}

}