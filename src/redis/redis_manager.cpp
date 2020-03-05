#include "redis_manager.h"
#include <iostream>
#include "config/group_store_format.h"

namespace bcm {

RedisDbManager::RedisDbManager()
{
}

bool RedisDbManager::setRedisDbConfig(const RedisPartitionMap& redisDbConfig) 
{
    if (redisDbConfig.empty()) {
        LOGE << "groupRedis config redis is null";
        return false;
    }
    for (auto& partition : redisDbConfig) {
        if (partition.second.empty()) {
            LOGE << "groupRedis config partition: " << partition.first << ", is null";
            return false;
        }
        std::vector<std::shared_ptr<RedisServer>> redisConns;
        for (uint32_t j=0; j<partition.second.size(); j++) {
            auto itr = partition.second.find(std::to_string(j));
            if (itr == partition.second.end()) {
                LOGE << "groupRedis config partition: " << partition.first << ", redis number: " << j << ", error";
                return false;
            }
            std::shared_ptr<RedisServer> ptrRedis = std::make_shared<RedisServer>(itr->second.ip, itr->second.port, itr->second.password, "");  // regkey 字段没用
            redisConns.emplace_back(ptrRedis);
        }
        m_redisPartitions[partition.first] = redisConns;
        m_currPartitionConn[partition.first] = 0;
        m_consistentHash.AddServer(partition.first);
    }
    return true;
}

bool RedisDbManager::hset(uint64_t gid, const std::string& key, const std::string& field, const std::string& value)
{
    std::string  partitionName;
    size_t numOfRedis;
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisByGid(gid, partitionName, numOfRedis);

    size_t loopCounter = 0;
    do {
        if (nullptr == ptrRedisServer) {
            return false;
        }

        std::shared_ptr<RedisConn>  ptrRedisConn = ptrRedisServer->getRedisConn();
        if (nullptr != ptrRedisConn) {
            bool isSuccess = ptrRedisConn->redisCmdArgs3(
                    std::string("HSET"),
                    key.c_str(), key.size(),
                    field.c_str(), field.size(),
                    value.c_str(), value.size());
            ptrRedisServer->freeRedisConn(ptrRedisConn);

            if (isSuccess) {
                return isSuccess;
            }
        }

        ptrRedisServer = getNextRedis(partitionName);
    } while (++loopCounter < numOfRedis);

    return false;
}

bool RedisDbManager::hmset(uint64_t gid, const std::string& key, const std::vector<HField>& values)
{
    std::string  partitionName;
    size_t numOfRedis;
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisByGid(gid, partitionName, numOfRedis);

    size_t loopCounter = 0;
    do {
        if (nullptr == ptrRedisServer) {
            return false;
        }

        std::shared_ptr<RedisConn>  ptrRedisConn = ptrRedisServer->getRedisConn();
        if (nullptr != ptrRedisConn) {
            bool isSuccess = ptrRedisConn->hmset(key, values);
            ptrRedisServer->freeRedisConn(ptrRedisConn);

            if (isSuccess) {
                return isSuccess;
            }
        }

        ptrRedisServer = getNextRedis(partitionName);
    } while (++loopCounter < numOfRedis);

    return false;
}

bool RedisDbManager::hget(uint64_t gid, const std::string& key, const std::string& field, std::string& value)
{
    std::string  partitionName;
    size_t numOfRedis;
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisByGid(gid, partitionName, numOfRedis);

    size_t loopCounter = 0;
    do {
        if (nullptr == ptrRedisServer) {
            return false;
        }

        std::shared_ptr<RedisConn>  ptrRedisConn = ptrRedisServer->getRedisConn();
        if (nullptr != ptrRedisConn) {
            bool isSuccess = ptrRedisConn->hget(key, field, value);
            ptrRedisServer->freeRedisConn(ptrRedisConn);

            if (isSuccess) {
                return isSuccess;
            }
        }

        ptrRedisServer = getNextRedis(partitionName);
    } while (++loopCounter < numOfRedis);

    return false;
}

bool RedisDbManager::hmget(uint64_t gid,
                           const std::string& key, 
                           const std::vector<std::string>& fields, 
                           std::map<std::string, std::string>& mapFieldValue)
{
    std::string  partitionName;
    size_t numOfRedis;
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisByGid(gid, partitionName, numOfRedis);

    size_t loopCounter = 0;
    do {
        if (nullptr == ptrRedisServer) {
            return false;
        }

        std::shared_ptr<RedisConn>  ptrRedisConn = ptrRedisServer->getRedisConn();
        if (nullptr != ptrRedisConn) {
            bool isSuccess = ptrRedisConn->hmget(key, fields, mapFieldValue);
            ptrRedisServer->freeRedisConn(ptrRedisConn);

            if (isSuccess) {
                return isSuccess;
            }
        }

        ptrRedisServer = getNextRedis(partitionName);
    } while (++loopCounter < numOfRedis);

    return false;
}

bool RedisDbManager::hdel(uint64_t gid, const std::string& key, const std::vector<std::string>& fields)
{
    std::string  partitionName;
    size_t numOfRedis;
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisByGid(gid, partitionName, numOfRedis);

    size_t loopCounter = 0;
    do {
        if (nullptr == ptrRedisServer) {
            return false;
        }

        std::shared_ptr<RedisConn>  ptrRedisConn = ptrRedisServer->getRedisConn();
        if (nullptr != ptrRedisConn) {
            bool isSuccess = ptrRedisConn->hdel(key, fields);
            ptrRedisServer->freeRedisConn(ptrRedisConn);

            if (isSuccess) {
                return isSuccess;
            }
        }

        ptrRedisServer = getNextRedis(partitionName);
    } while (++loopCounter < numOfRedis);

    return false;
}

bool RedisDbManager::zadd(uint64_t gid, const std::string& key, const std::string& mem, const int64_t score)
{
    std::string  partitionName;
    size_t numOfRedis;
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisByGid(gid, partitionName, numOfRedis);

    size_t loopCounter = 0;
    do {
        if (nullptr == ptrRedisServer) {
            return false;
        }

        std::shared_ptr<RedisConn>  ptrRedisConn = ptrRedisServer->getRedisConn();
        if (nullptr != ptrRedisConn) {
            bool isSuccess = ptrRedisConn->zadd(key, mem, score);
            ptrRedisServer->freeRedisConn(ptrRedisConn);

            if (isSuccess) {
                return isSuccess;
            }
        }

        ptrRedisServer = getNextRedis(partitionName);
    } while (++loopCounter < numOfRedis);

    return false;
}

std::shared_ptr<RedisServer> RedisDbManager::getRedisByGid(uint64_t gid,
                                                           std::string& outPartitionName, size_t& redisSize)
{
    outPartitionName  = m_consistentHash.GetServer(gid);
    if ("" == outPartitionName) {
        LOGE << "partition name is empty in consistent hash !";
        return nullptr;
    }

    std::shared_lock<std::shared_timed_mutex> l(m_currPartitionConnMutex);

    auto itRedisServer = m_redisPartitions.find(outPartitionName);
    if (itRedisServer == m_redisPartitions.end()) {
        return nullptr;
    }

    std::vector<std::shared_ptr<RedisServer>>& partitionRedisVector = itRedisServer->second;
    redisSize = partitionRedisVector.size();

    int32_t redisIndex = 0;
    auto itr = m_currPartitionConn.find(outPartitionName);
    if (itr != m_currPartitionConn.end()) {
        redisIndex = itr->second;
    }

    return partitionRedisVector[redisIndex];
}

std::shared_ptr<RedisServer> RedisDbManager::getRedisByKey(const std::string& hashKey,
                                                               std::string& outPartitionName, size_t& redisSize)
{
    outPartitionName  = m_consistentHash.GetServer(hashKey);
    if ("" == outPartitionName) {
        LOGE << "partition name is empty in consistent hash !";
        return nullptr;
    }

    std::shared_lock<std::shared_timed_mutex> l(m_currPartitionConnMutex);

    auto itRedisServer = m_redisPartitions.find(outPartitionName);
    if (itRedisServer == m_redisPartitions.end()) {
        return nullptr;
    }

    std::vector<std::shared_ptr<RedisServer>>& partitionRedisVector = itRedisServer->second;
    redisSize = partitionRedisVector.size();

    int32_t redisIndex = 0;
    auto itr = m_currPartitionConn.find(outPartitionName);
    if (itr != m_currPartitionConn.end()) {
        redisIndex = itr->second;
    }

    return partitionRedisVector[redisIndex];
}

std::shared_ptr<RedisServer> RedisDbManager::getNextRedis(const std::string& partitionName)
{
    std::unique_lock<std::shared_timed_mutex> l(m_currPartitionConnMutex);

    std::shared_ptr<RedisServer> ptrRedisServer;
    auto itRedisServer = m_redisPartitions.find(partitionName);
    if (itRedisServer == m_redisPartitions.end()) {
        return nullptr;
    }
    std::vector<std::shared_ptr<RedisServer>>& partitionRedisVector = itRedisServer->second;
    size_t redisSize = partitionRedisVector.size();

    auto itPartId = m_currPartitionConn.find(partitionName);
    if (itPartId == m_currPartitionConn.end()) {
        return nullptr;
    }
    itPartId->second++;
    itPartId->second = itPartId->second%redisSize;

    return partitionRedisVector[itPartId->second];
}

int32_t RedisDbManager::incr(const std::string& key, uint64_t& newValue) {
    return incr(key, key, newValue);
}

int32_t RedisDbManager::incr(const std::string& hashKey, const std::string& key, uint64_t& newValue)
{
    std::string  partitionName;
    size_t numOfRedis;
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisByKey(hashKey, partitionName, numOfRedis);

    size_t loopCounter = 0;
    do {
        if (nullptr == ptrRedisServer) {
            return -1;
        }

        std::shared_ptr<RedisConn>  ptrRedisConn = ptrRedisServer->getRedisConn();
        if (nullptr != ptrRedisConn) {

            int32_t res = ptrRedisConn->incr(key, newValue);
            ptrRedisServer->freeRedisConn(ptrRedisConn);

            if (res >= 0) {
                return res;
            }
        }

        ptrRedisServer = getNextRedis(partitionName);
    } while (++loopCounter < numOfRedis);

    return -1;
}

bool RedisDbManager::del(const std::string& key)
{
    return del(key, key);
}
bool RedisDbManager::del(const std::string& hashKey, const std::string& key)
{
    std::string  partitionName;
    size_t numOfRedis;
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisByKey(hashKey, partitionName, numOfRedis);

    size_t loopCounter = 0;
    do {
        if (nullptr == ptrRedisServer) {
            return false;
        }

        std::shared_ptr<RedisConn>  ptrRedisConn = ptrRedisServer->getRedisConn();
        if (nullptr != ptrRedisConn) {

            bool res = ptrRedisConn->del(key);
            ptrRedisServer->freeRedisConn(ptrRedisConn);

            if (res) {
                return res;
            }
        }

        ptrRedisServer = getNextRedis(partitionName);
    } while (++loopCounter < numOfRedis);

    return false;
}

int32_t RedisDbManager::expire(const std::string& key, uint32_t timeout)
{
    return expire(key, key, timeout);
}
int32_t RedisDbManager::expire(const std::string& hashKey, const std::string& key, uint32_t timeout)
{
    std::string  partitionName;
    size_t numOfRedis;
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisByKey(hashKey, partitionName, numOfRedis);

    size_t loopCounter = 0;
    do {
        if (nullptr == ptrRedisServer) {
            return -1;
        }

        std::shared_ptr<RedisConn>  ptrRedisConn = ptrRedisServer->getRedisConn();
        if (nullptr != ptrRedisConn) {
            int32_t res = ptrRedisConn->expire(key, timeout);
            ptrRedisServer->freeRedisConn(ptrRedisConn);

            if (res >= 0) {
                return res;
            }
        }

        ptrRedisServer = getNextRedis(partitionName);
    } while (++loopCounter < numOfRedis);

    return -1;
}

int32_t RedisDbManager::ttl(const std::string& key)
{
    return ttl(key, key);
}

int32_t RedisDbManager::ttl(const std::string& hashKey, const std::string& key)
{
    std::string  partitionName;
    size_t numOfRedis;
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisByKey(hashKey, partitionName, numOfRedis);

    size_t loopCounter = 0;
    do {
        if (nullptr == ptrRedisServer) {
            return -1;
        }

        std::shared_ptr<RedisConn>  ptrRedisConn = ptrRedisServer->getRedisConn();
        if (nullptr != ptrRedisConn) {
            int32_t res = ptrRedisConn->ttl(key);
            ptrRedisServer->freeRedisConn(ptrRedisConn);

            if (res >= 0) {
                return res;
            }
        }

        ptrRedisServer = getNextRedis(partitionName);
    } while (++loopCounter < numOfRedis);

    return -1;
}

bool RedisDbManager::set(const std::string& hashKey, const std::string& key,
                            const std::string& value, const int exptime)
{
    std::string  partitionName;
    size_t numOfRedis;
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisByKey(hashKey, partitionName, numOfRedis);

    size_t loopCounter = 0;
    do {
        if (nullptr == ptrRedisServer) {
            return false;
        }

        std::shared_ptr<RedisConn>  ptrRedisConn = ptrRedisServer->getRedisConn();
        if (nullptr != ptrRedisConn) {
            bool isSuccess = ptrRedisConn->set(key, value, exptime);
            ptrRedisServer->freeRedisConn(ptrRedisConn);

            if (isSuccess) {
                return isSuccess;
            }
        }

        ptrRedisServer = getNextRedis(partitionName);
    } while (++loopCounter < numOfRedis);

    return false;
}

bool RedisDbManager::set(const std::string& key, const std::string& value, const int exptime)
{
    return set(key, key, value, exptime);
}

bool RedisDbManager::get(const std::string& hashKey, const std::string& key, std::string& value)
{
    std::string partitionName;
    size_t numOfRedis;
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisByKey(hashKey, partitionName, numOfRedis);

    size_t loopCounter = 0;
    do {
        if (nullptr == ptrRedisServer) {
            return false;
        }

        std::shared_ptr<RedisConn> ptrRedisConn = ptrRedisServer->getRedisConn();
        if (nullptr != ptrRedisConn) {
            bool isSuccess = ptrRedisConn->get(key, value);
            ptrRedisServer->freeRedisConn(ptrRedisConn);

            if (isSuccess) {
                return isSuccess;
            }
        }

        ptrRedisServer = getNextRedis(partitionName);
    } while (++loopCounter < numOfRedis);

    return false;
}

bool RedisDbManager::get(const std::string& key, std::string& value)
{
    return get(key, key, value);
}

void RedisDbManager::updateRedisConnPeriod()
{
    std::shared_ptr<RedisServer> ptrRedisServer;
    std::shared_ptr<RedisConn> ptrRedisConn;

    std::unordered_map<std::string, int> tmpCurrPartitionConn;
    {
        std::shared_lock<std::shared_timed_mutex> l(m_currPartitionConnMutex);
        tmpCurrPartitionConn = m_currPartitionConn;
    }

    for (auto& p : tmpCurrPartitionConn) {
        for (int redisId = 0; redisId <= p.second; redisId++) {
            ptrRedisServer = m_redisPartitions[p.first][redisId];
            ptrRedisConn = ptrRedisServer->getRedisConn();
            if (ptrRedisConn == nullptr) {
                LOGE << "redis manager run failed to get available redis connection: partitionName " << p.first << ", redisIndex " << redisId;
                continue;
            }

            bool isSuccess = ptrRedisConn->set(REDISDB_KEY_GROUP_REDIS_ACTIVE, "active", 15);
            ptrRedisServer->freeRedisConn(ptrRedisConn);
            if (isSuccess && redisId < p.second) {
                std::unique_lock<std::shared_timed_mutex> l(m_currPartitionConnMutex);
                m_currPartitionConn[p.first] = redisId;
                break;
            }
        }
    }
}

} // namespace bcm

