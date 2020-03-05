#pragma once
#include <set>
#include <list>
#include <map>
#include <cstdlib>
#include <thread>
#include <string>
#include <vector>
#include <event.h>
#include <unordered_map>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <config/redis_config.h>
#include <mutex>
#include "iasync_redis_event.h"

namespace bcm{

// Entry for a hash field
struct HField {
    std::string field;
    std::string value;
    explicit HField(const std::string& f, const std::string& v) : field(f), value(v) {}
};

struct ZSetMemberScore {
    std::string member;
    int64_t     score;
};

class RedisConn {
public:
    RedisConn(const std::string& host, const int port, const std::string& password);
    ~RedisConn();

public:
    // string
    bool get(const std::string& strKey, std::string& strValue);
    bool set(const std::string& key, const std::string& value, const int exptime = 0);
    bool mget(const std::set<std::string>& setKeys,
              std::unordered_map<std::string, std::string>& mapKeyValues);

    // hash
    bool hget(const std::string& key, const std::string& field, std::string& value);
    bool hmget(const std::string& key, const std::vector<std::string>& fields,
               std::map<std::string, std::string>& mapFieldValue);
    bool hlen(const std::string& strKey, uint64_t& num);
    // HSCAN command:
    // specify "" to 'pattern' to ignore MATCH keyword;
    // specify 0 to 'count' to ignore COUNT keyword;
    // NOTE:
    // 'results' will be cleared before any valid records being filled up
    bool hscan(const std::string& key,
               const std::string& cursor, const std::string& pattern,
               uint32_t count,
               std::string& new_cursor,
               std::map<std::string, std::string>& results);
    bool hmset(const std::string& key, const std::vector<HField>& values);

    bool hdel(const std::string& key, const std::vector<std::string>& fieldList);

    // set
    bool smembers(const std::string& key, std::vector<std::string>& memberList);
    bool smembersBatch(const std::set<std::string>& setKeys,
                       std::unordered_map<std::string, std::vector<std::string>>& mapKeyMembers);

    // zset/sortset
    bool zadd(const std::string& key, const std::string& mem, const int64_t& score);
    bool zrem(const std::string& key, const std::vector<std::string>& memberList);

    bool getMemsByScoreWithLimit(
            const std::string& key,
            const int64_t min,
            const int64_t max,
            const uint32_t offset,
            const uint32_t limit,
            std::vector<ZSetMemberScore>& mems);

    // pub/sub
    bool pubsub(const std::string& strTopic, std::set<std::string>& setTopics);
    bool publishRes(const std::string& channel, const std::string& message);
    bool publishBatch(std::set<std::string>& setCmds);


    bool redisCmdArgs0(const std::string& strCmd);
    bool redisCmdArgs1(const std::string& strCmd, const char* pchKey, int nKeySize);
    bool redisCmdArgs2(const std::string& strCmd, const char* pchKey, int nKeySize, const char* pchValue, int nValueSize);
    bool redisCmdArgs3(const std::string& strCmd, const char* pchKey, int nKeySize,
                       const char* pchField, int nFieldSize, const char* pchValue, int nValueSize);

    // Add for decoupling online and offline modules
    bool unifiedCall(
        const std::string& fullCmd);

    // Return
    //     -1: communication error
    //      0: key is not a valid integer
    // Others: success
    int32_t incr(const std::string& key, uint64_t& newValue);

    // Return
    //     -1: communication error
    //      0: key does not exist
    // Others: timeout was set successfully
    int32_t expire(const std::string& key, uint32_t timeout);

    // Return
    //     -1: communication error
    //      0: key does not exist or has no associated expire
    // Others: TTL in second
    int64_t ttl(const std::string& key);
    
    bool del(const std::string& key);

    bool connectRedis();
    uint32_t info_uptime();

private:
    bool reConnectRedis();
    void freeConnect();

    bool isReplySuccess(const redisReply* pReply);
    std::string getReplyError(const redisReply* reply);
    std::string getStringType(const redisReply* reply);

private:
    std::string m_redisHost;
    int m_redisPort;
    std::string m_redisPassword;
    int64_t m_dwLastActiveTime;
    
    redisContext* m_pRedisContext;
};

class RedisServer{
public:
    RedisServer(const std::string& host, const int port, const std::string& password, const std::string& keepaliveKey);
    ~RedisServer() {}

    std::shared_ptr<RedisConn> getRedisConn();
    void freeRedisConn(std::shared_ptr<RedisConn> pRedisConn);
    void syncRedisKeepAlive();

    std::string getRedisHost() { return m_redisHost; }
    int getRedisPort() { return m_redisPort; }
    std::string getRedisPassword() { return m_redisPassword;}
	std::string getKeepaliveKey() { return m_keepaliveKey;}

private:
    std::string m_redisHost;
    int m_redisPort;
    std::string m_redisPassword;
	std::string m_keepaliveKey;

    std::recursive_mutex m_mutexConnectList;
    std::list<std::shared_ptr<RedisConn>> m_redisConnList;
};

class RedisClientSync {
public:
    RedisClientSync();
    ~RedisClientSync();

    static RedisClientSync* Instance()
    {
        static RedisClientSync gs_instance;
        return &gs_instance;
    }

    static RedisClientSync* OnlineInstance()
    {
        static RedisClientSync online_instance;
        return &online_instance;
    }

    static RedisClientSync* OfflineInstance()
    {
        static RedisClientSync offline_instance;
        return &offline_instance;
    }

	void setRedisConfig(const std::vector<RedisConfig>& redisHosts);
    bool setRedisConfig(const std::map<int32_t, RedisConfig>& redisHosts);

    //redis sync api.
    // string
    bool set(const std::string& key, const std::string& value, const int extime = 0);
    bool get(const std::string& key, std::string& value);
    bool del(const std::string& key);
    bool mget(const std::string& strPrekey,
              const std::set<std::string>& setKeys,
              std::unordered_map<std::string, std::string>& mapKeyValues);
    bool mget(const std::string& strPrekey,
              const std::vector<std::string>& setKeys,
              std::unordered_map<std::string, std::string>& mapKeyValues);

    // hash
    bool hset(const std::string& key, const std::string& field, const std::string& value);
    bool hget(const std::string& key, const std::string& field, std::string& value);
    bool hmget(const std::string& key, const std::vector<std::string>& fields,
               std::map<std::string, std::string>& mapFieldValue);
    bool hdel(const std::string& key, const std::string& field);


    bool sadd(const std::string& key, const std::string& member);
    bool srem(const std::string& key, const std::string& member);
    bool smembers(const std::string& key, std::vector<std::string>& memberList);
    bool smembersBatch(const std::set<std::string>& setKeys, std::unordered_map<std::string, std::vector<std::string>>& mapKeyMembers);
    bool pubsub(const std::string& strTopic, std::set<std::string>& setTopics);
    bool publishBatch(std::map<std::string, std::string>& mapChannelMessage);
    uint32_t info_uptime();
    
    bool publish(const std::string& channel, const std::string& message);
    bool publishRes(const std::string& channel, const std::string& message);//publish and care total subscribers.
    bool ppublish(const std::vector<std::string>& channels, const std::vector<std::string>& messages);

    // Add for decoupling online and offline modules
    bool hsetnx(
        const std::string& key,
        const std::string& field,
        const std::string& value);
    bool hlen(
        const std::string& key,
        uint64_t& num);
    // HSCAN command:
    // specify "" to 'pattern' to ignore MATCH keyword;
    // specify 0 to 'count' to ignore COUNT keyword;
    // NOTE:
    // 'results' will be cleared before any valid records being filled up
    bool hscan(
        const std::string& key,
        const std::string& cursor,
        const std::string& pattern,
        uint32_t count,
        std::string& new_cursor,
        std::map<std::string, std::string>& results);
    bool hmset(
        const std::string& key,
        const std::vector<HField>& values);

    //
    bool get(int32_t dbIndex, const std::string& key, std::string& value);

    bool hscan(
            int32_t dbIndex,
            const std::string& key,
            const std::string& cursor,
            const std::string& pattern,
            uint32_t count,
            std::string& new_cursor,
            std::map<std::string, std::string>& results);
    bool hmset(
            int32_t dbIndex,
            const std::string& key,
            const std::vector<HField>& values);

    bool hmget(int32_t dbIndex, const std::string& key,
               const std::vector<std::string>& fields,
               std::map<std::string, std::string>& mapFieldValue);

    bool hdel(int32_t dbIndex, const std::string& key, const std::vector<std::string>& fieldList);

    // zset/sortset
    bool zadd(int32_t dbIndex, const std::string& key,
              const std::string& mem, const int64_t& score);
    bool zrem(int32_t dbIndex, const std::string& key,
              const std::vector<std::string>& memberList);

    bool getMemsByScoreWithLimit(int32_t dbIndex,
            const std::string& key,
            const int64_t min,
            const int64_t max,
            const uint32_t offset,
            const uint32_t limit,
            std::vector<ZSetMemberScore>& mems);

    // Return
    //     -1: communication error
    //      0: key is not a valid integer
    // Others: success
    int32_t incr(const std::string& key, uint64_t& newValue);

    // Return
    //     -1: communication error
    //      0: key does not exist
    // Others: timeout was set successfully
    int32_t expire(const std::string& key, uint32_t timeout);

    // Return
    //     -1: communication error
    //      0: key does not exist or has no associated expire
    // Others: TTL in second
    int64_t ttl(const std::string& key);


private:
    void doAuthRedis();
    bool connectRedisLater();
    std::shared_ptr<RedisServer> getRedisServer(const std::string& key);
    std::shared_ptr<RedisServer> getRedisServerById(int32_t redisId);

private:
    std::recursive_mutex m_mutexRedisList;
    std::map<int32_t, std::shared_ptr<RedisServer>> m_vecRedisClusterList;
};

class RedisClientAsync {
public:
    RedisClientAsync();
    ~RedisClientAsync();

    static RedisClientAsync* Instance()
    {
        static RedisClientAsync gs_instance;
        return &gs_instance;
    }

	bool run();
    bool createAsyncConn();
    bool asyncRedisLoop();
    void setAsyncHandler(IAsyncRedisEvent* pAsyncHandler);
	void setRedisConfig(const std::vector<RedisConfig>& redisHosts);
    void startAsyncRedisThread();
    
    //channel async subscribe.
    bool subscribe(const std::string& channel);
    bool unsubscribe(const std::string& channel);
    bool psubscribe(const std::string& channel);

private:
    static void onReCreateAsyncConn(evutil_socket_t fd, short events, void *arg);
    static void onAuthRes(redisAsyncContext* pRedisContext, void* pvRedisReply, void* pPrivateData);
    static void onRedisMessage(redisAsyncContext* pRedisContext, void* pvRedisReply, void* pPrivateData);
    static void onRedisAyncConnected(const redisAsyncContext *pRedisContext, int status);
    static void onRedisDisconnected(const redisAsyncContext *pRedisContext, int status);
    static void onTimeout5s(evutil_socket_t fd, short events, void *arg);    

    void doAuthRedis();
    bool connectRedisLater();
    bool asyncCommand(const std::string& strAsyncCmd);
    std::shared_ptr<RedisServer> getRedisServer(const std::string& key);
    
private:
	std::thread m_redisThread;
    struct event *m_pLaterConnectEvent;
    struct event_base *m_pAsyncEventBase;
    redisAsyncContext *m_pAsyncRedisConnect;
    IAsyncRedisEvent* m_pAsyncAppServer;

    static bool m_isAsyncConnected;
    std::shared_ptr<RedisServer> m_ptrRedisServer;
};

}
