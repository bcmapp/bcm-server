#pragma once

#include "utils/consistent_hash.h"
#include <unordered_map>
#include "config/redis_config.h"
#include "redis/hiredis_client.h"
#include <shared_mutex>


namespace bcm
{

class RedisDbManager {
    typedef std::unordered_map<std::string, std::unordered_map<std::string, RedisConfig>> RedisPartitionMap;
public:
    RedisDbManager();

    static RedisDbManager* Instance()
    {
        static RedisDbManager gs_instance;
        return &gs_instance;
    }

    bool setRedisDbConfig(const RedisPartitionMap& redisDbConfig);

    bool hset(uint64_t gid, const std::string& key, const std::string& field, const std::string& value);
    bool hmset(uint64_t gid, const std::string& key, const std::vector<HField>& values);
    bool hget(uint64_t gid, const std::string& key, const std::string& field, std::string& value);
    bool hmget(uint64_t gid, 
               const std::string& key, 
               const std::vector<std::string>& fields, 
               std::map<std::string, std::string>& mapFieldValue);
    bool hdel(uint64_t gid, const std::string& key, const std::vector<std::string>& fields);
    bool zadd(uint64_t gid, const std::string& key, const std::string& mem, const int64_t score);  // score 作为引用

    int32_t incr(const std::string& hashKey, const std::string& key, uint64_t& newValue);
    int32_t expire(const std::string& hashKey, const std::string& key, uint32_t timeout);
    int32_t ttl(const std::string& hashKey, const std::string& key);
    bool del(const std::string& hashKey, const std::string& key);

    bool set(const std::string& hashKey, const std::string& key, const std::string& value, const int exptime = 0);
    bool get(const std::string& hashKey, const std::string& key, std::string& value);

    bool set(const std::string& key, const std::string& value, const int exptime = 0);
    bool get(const std::string& key, std::string& value);
    bool del(const std::string& key);
    int32_t incr(const std::string& key, uint64_t& newValue);
    int32_t expire(const std::string& key, uint32_t timeout);
    int32_t ttl(const std::string& key);

    void updateRedisConnPeriod();

private:
    std::shared_ptr<RedisServer> getRedisByGid(uint64_t gid,
                                               std::string& outPartitionName, size_t& redisSize);
    std::shared_ptr<RedisServer> getRedisByKey(const std::string& hashKey,
                                               std::string& outPartitionName, size_t& redisSize);
    std::shared_ptr<RedisServer> getNextRedis(const std::string& partitionName);
private:
    std::unordered_map<std::string, std::vector<std::shared_ptr<RedisServer>>> m_redisPartitions;
    std::unordered_map<std::string, int32_t> m_currPartitionConn;
    std::shared_timed_mutex m_currPartitionConnMutex;
    UserConsistentFnvHash m_consistentHash;
};

} // namespace bcm
