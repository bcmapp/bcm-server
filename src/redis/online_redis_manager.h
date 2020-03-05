#pragma once

#include "config/redis_config.h"
#include "redis/async_conn.h"
#include "utils/consistent_hash.h"
#include <event2/event_struct.h>
#include <map>
#include <unordered_map>
#include <string>

namespace bcm {

using namespace redis;

class RedisPartition;

static const std::string kKeepAliveChannel = "onlineRedis:keepAlive";
static const std::string kOnlineRedisServiceName = "OnlineRedisService";
static const std::string kOnlineRedisTopicName = "availability";

class OnlineRedisManager {
typedef std::map<std::string, std::vector<RedisConfig>> Partition2RedisCfgMap;

public:
    OnlineRedisManager();
    ~OnlineRedisManager();

    static OnlineRedisManager* Instance() 
    {
        static OnlineRedisManager gs_instance;
        return &gs_instance;
    }

    bool init(const Partition2RedisCfgMap& redisConfig);
    void start();

    bool subscribe(const std::string& chan, AsyncConn::ISubscriptionHandler* handler);
    bool subscribe(const std::string& hashKey, const std::string& chan, AsyncConn::ISubscriptionHandler* handler);
    bool psubscribe(const std::string& chan, AsyncConn::ISubscriptionHandler* handler);
    bool psubscribe(const std::string& hashKey, const std::string& chan, AsyncConn::ISubscriptionHandler* handler);

    bool unsubscribe(const std::string& chan);
    bool unsubscribe(const std::string& hashKey, const std::string& chan);
    bool punsubscribe(const std::string& chan);
    
    bool publish(const std::string& channel, const std::string& message, AsyncConn::ReplyHandler&& handler = nullptr);
    bool publish(const std::string& hashKey, const std::string& channel, 
                 const std::string& message, AsyncConn::ReplyHandler&& handler = nullptr);

    bool isSubscribed(const std::string& chan);
    bool isSubscribed(const std::string& hashKey, const std::string& chan);

private:
    static constexpr int32_t kKeepAliveInterval = 30; // s
    static void onKeepAlive(evutil_socket_t fd, short events, void *arg) {
        boost::ignore_unused(fd, events);
        OnlineRedisManager* pThis = reinterpret_cast<OnlineRedisManager*>(arg);
        pThis->runKeepAlive();
        LOGT << "online redis manager keep alive";
    }

    std::shared_ptr<RedisPartition> getPartitionByHashKey(const std::string hashKey);
    void runKeepAlive();

private:
    struct event_base* m_eb;
    struct event m_evtKeepAlive;
    std::thread m_thread;
    std::unordered_map<std::string, std::shared_ptr<RedisPartition>> m_partitions;
    UserConsistentFnvHash m_consistentHash;
};

}