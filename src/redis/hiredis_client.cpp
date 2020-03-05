#include <signal.h>
#include <iostream>
#include "utils/log.h"
#include "utils/time.h"
#include "utils/consistent_hash.h"
#include "utils/thread_utils.h"
#include <hiredis/adapters/libevent.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include "hiredis_client.h"
#include "libevent_task.h"

using namespace bcm;

#define REDIS_NEGATIVE_INFINITY 0
#define REDIS_POSITIVE_INFINITY -1
#define REDIS_NEGATIVE_INFINITY_STR "-inf"
#define REDIS_POSITIVE_INFINITY_STR "+inf"


static const int kRedisConnectLater = 3;//3s
bool RedisClientAsync::m_isAsyncConnected = false;

RedisClientSync::RedisClientSync() 
{
}


RedisClientSync::~RedisClientSync()
{
}

void RedisClientSync::setRedisConfig(const std::vector<RedisConfig>& redisHosts)
{
    {
        std::unique_lock<std::recursive_mutex> mutexRecu(m_mutexRedisList);
        m_vecRedisClusterList.clear();
    }

    int32_t redisIndex = 0;
    for (auto& redisConfig : redisHosts) {
        std::shared_ptr<RedisServer> ptrRedis = std::make_shared<RedisServer>(redisConfig.ip, redisConfig.port, redisConfig.password, redisConfig.regkey);
        {
            std::unique_lock<std::recursive_mutex> mutexRecu(m_mutexRedisList);
            m_vecRedisClusterList[redisIndex] = ptrRedis;
        }
        redisIndex++;

        LOGI << "parsed target redis server.(" << redisConfig.ip << ":" << redisConfig.port << " " << redisConfig.password << ")";
    }
}

bool RedisClientSync::setRedisConfig(const std::map<int32_t, RedisConfig>& redisHosts)
{
    {
        std::unique_lock<std::recursive_mutex> mutexRecu(m_mutexRedisList);
        m_vecRedisClusterList.clear();
    }

    int32_t redisIndex = 0;
    for (auto& redisConfig : redisHosts) {

        if (redisConfig.first != redisIndex) {
            return false;
        }

        std::shared_ptr<RedisServer> ptrRedis = std::make_shared<RedisServer>(redisConfig.second.ip,
                                                                              redisConfig.second.port,
                                                                              redisConfig.second.password,
                                                                              redisConfig.second.regkey);
        {
            std::unique_lock<std::recursive_mutex> mutexRecu(m_mutexRedisList);
            m_vecRedisClusterList[redisConfig.first] = ptrRedis;
        }
        redisIndex++;

        LOGI << "parsed target redis server. ip: "
                << redisConfig.second.ip << ":" << redisConfig.second.port
                << ", pwd: " << redisConfig.second.password << ")";
    }

    return true;
}

std::shared_ptr<RedisServer> RedisClientSync::getRedisServer(const std::string& key)
{
    std::unique_lock<std::recursive_mutex> mutexRecu(m_mutexRedisList);
    int nRedisSize = m_vecRedisClusterList.size();
    if (nRedisSize == 0) {
        return nullptr;
    }

    //find the cluster redis pool according to key.
    uint32_t uHashValue = FnvHashFunction::hash(key.data(), key.length());
    int nIndex = uHashValue % nRedisSize;

    if (m_vecRedisClusterList.find(nIndex) == m_vecRedisClusterList.end()) {
        return nullptr;
    }

    return m_vecRedisClusterList[nIndex];
}

std::shared_ptr<RedisServer> RedisClientSync::getRedisServerById(int32_t redisId)
{
    std::unique_lock<std::recursive_mutex> mutexRecu(m_mutexRedisList);
    int nRedisSize = m_vecRedisClusterList.size();
    if (redisId >= nRedisSize) {
        return nullptr;
    }

    if (m_vecRedisClusterList.find(redisId) == m_vecRedisClusterList.end()) {
        return nullptr;
    }

    return m_vecRedisClusterList[redisId];
}

bool RedisClientSync::get(const std::string& key, std::string& value)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[GET] failed to get available redis connection.";
        return false;
    }

    //excute redis command.
    bool isSuccess = pRedisConn->get(key, value);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::get(int32_t dbIndex, const std::string& key, std::string& value)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServerById(dbIndex);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[GET] failed to get available redis connection.";
        return false;
    }

    //excute redis command.
    bool isSuccess = pRedisConn->get(key, value);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::set(const std::string& key, const std::string& value, const int exptime)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[SET] failed to get available redis connection.";
        return false;
    }

    bool isSuccess = pRedisConn->set(key, value, exptime);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::del(const std::string& key)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[DEL] failed to get available redis connection.";
        return false;
    }

    bool isSuccess = pRedisConn->redisCmdArgs1(std::string("DEL"), key.c_str(), key.size());
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::mget(const std::string& strPrekey, const std::set<std::string>& setKeys, std::unordered_map<std::string, std::string>& mapKeyValues)
{
    //distinct the redis keys.
    std::map<std::shared_ptr<RedisServer>, std::set<std::string>> mapNodeKeys;
    for (std::set<std::string>::const_iterator cnitSetValue = setKeys.begin(); cnitSetValue != setKeys.end(); cnitSetValue++) {
        std::string strRedisKey = strPrekey + (*cnitSetValue);
        std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(strRedisKey);
        if (ptrRedisServer != nullptr) {
            mapNodeKeys[ptrRedisServer].insert(strRedisKey);
        }
    }

    if (mapNodeKeys.empty()) {
        return false;
    }

    for (std::map<std::shared_ptr<RedisServer>, std::set<std::string>>::iterator itMapNodeKeys = mapNodeKeys.begin();
        itMapNodeKeys != mapNodeKeys.end(); ++itMapNodeKeys) {
        std::shared_ptr<RedisConn> pRedisConn = itMapNodeKeys->first->getRedisConn();
        if (pRedisConn != nullptr) {
            pRedisConn->mget(itMapNodeKeys->second, mapKeyValues);
        }
    }

    return (mapKeyValues.empty() ? false : true);
}

bool RedisClientSync::mget(const std::string& strPrekey, const std::vector<std::string>& setKeys, std::unordered_map<std::string, std::string>& mapKeyValues)
{
    //distinct the redis keys.
    std::map<std::shared_ptr<RedisServer>, std::set<std::string>> mapNodeKeys;
    for (std::vector<std::string>::const_iterator cnitSetValue = setKeys.begin(); cnitSetValue != setKeys.end(); cnitSetValue++) {
        std::string strRedisKey = strPrekey + (*cnitSetValue);
        std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(strRedisKey);
        if (ptrRedisServer != nullptr) {
            mapNodeKeys[ptrRedisServer].insert(strRedisKey);
        }
    }

    if (mapNodeKeys.empty()) {
        return false;
    }

    for (std::map<std::shared_ptr<RedisServer>, std::set<std::string>>::iterator itMapNodeKeys = mapNodeKeys.begin();
        itMapNodeKeys != mapNodeKeys.end(); ++itMapNodeKeys) {
        std::shared_ptr<RedisConn> pRedisConn = itMapNodeKeys->first->getRedisConn();
        if (pRedisConn != nullptr) {
            pRedisConn->mget(itMapNodeKeys->second, mapKeyValues);
        }
    }

    return (mapKeyValues.empty() ? false : true);
}

bool RedisClientSync::hset(const std::string& key, const std::string& field, const std::string& value)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[HSET] failed to get available redis connection.";
        return false;
    }

    bool isSuccess = pRedisConn->redisCmdArgs3(std::string("HSET"), key.c_str(), key.size(), field.c_str(),  field.size(), value.c_str(), value.size());
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::hget(const std::string& key, const std::string& field, std::string& value)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[HGET] failed to get available redis connection.";
        return false;
    }

    bool isSuccess = pRedisConn->hget(key, field, value);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::hmget(const std::string& key, const std::vector<std::string>& fields, std::map<std::string, std::string>& mapFieldValue)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[HMGET] failed to get available redis connection.";
        return false;
    }

    bool isSuccess = pRedisConn->hmget(key, fields, mapFieldValue);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::hdel(const std::string& key, const std::string& field)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[HDEL] failed to get available redis connect.";
        return false;
    }

    bool isSuccess = pRedisConn->redisCmdArgs2(std::string("HDEL"), key.c_str(), key.size(), field.c_str(), field.size());
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::sadd(const std::string& key, const std::string& member)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[SADD] failed to get available redis connection.";
        return false;
    }

    bool isSuccess = pRedisConn->redisCmdArgs2(std::string("SADD"), key.c_str(), key.size(), member.c_str(), member.size());
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::srem(const std::string& key, const std::string& member)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[SREM] failed to get available redis connection.";
        return false;
    }

    bool isSuccess = pRedisConn->redisCmdArgs2(std::string("SREM"), key.c_str(), key.size(), member.c_str(), member.size());
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::smembers(const std::string& key, std::vector<std::string>& memberList)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[SMEMBERS] failed to get available redis connection.";
        return false;
    }

    bool isSuccess = pRedisConn->smembers(key, memberList);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::smembersBatch(const std::set<std::string>& setKeys, std::unordered_map<std::string, std::vector<std::string>>& mapKeyMembers)
{
    LOGI << "receive smembers batch request.(keys:" << setKeys.size() << ")";

    //distinct the redis keys.
    std::map<std::shared_ptr<RedisServer>, std::set<std::string>> mapNodeKeys;
    for (std::set<std::string>::const_iterator cnitSetValue = setKeys.begin(); cnitSetValue != setKeys.end(); cnitSetValue++) {
        std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(*cnitSetValue);
        if (ptrRedisServer != nullptr) {
            mapNodeKeys[ptrRedisServer].insert(*cnitSetValue);
        }
    }

    if (mapNodeKeys.empty()) {
        return false;
    }

    bool isSuccess = true;
    for (std::map<std::shared_ptr<RedisServer>, std::set<std::string>>::iterator itMapNodeKeys = mapNodeKeys.begin();
        itMapNodeKeys != mapNodeKeys.end(); ++itMapNodeKeys) {
        std::shared_ptr<RedisConn> pRedisConn = itMapNodeKeys->first->getRedisConn();
        if (pRedisConn != nullptr) {
            isSuccess &= pRedisConn->smembersBatch(itMapNodeKeys->second, mapKeyMembers);
            itMapNodeKeys->first->freeRedisConn(pRedisConn);
        }
    }

    return isSuccess;
}

bool RedisClientSync::publish(const std::string& channel, const std::string& message)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(channel);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[PUBLISH] failed to get available redis connection.";
        return false;
    }

    bool isSuccess = pRedisConn->redisCmdArgs2(std::string("PUBLISH"), channel.c_str(), channel.size(), message.c_str(), message.size());
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::publishRes(const std::string& channel, const std::string& message)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(channel);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[PUBLISHRES] failed to get available redis connection.";
        return false;
    }

    bool isSuccess = pRedisConn->publishRes(channel, message);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::pubsub(const std::string& strTopic, std::set<std::string>& setTopics)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(strTopic);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[PUBSUB] failed to get available redis connection.";
        return false;
    }

    bool isSuccess = pRedisConn->pubsub(strTopic, setTopics);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;    
}

bool RedisClientSync::publishBatch(std::map<std::string, std::string>& mapChannelMessage)
{
    std::map<std::shared_ptr<RedisServer>, std::set<std::string>> mapNodeCmds;
    for (std::map<std::string, std::string>::iterator itMapChannelMsg = mapChannelMessage.begin(); 
        itMapChannelMsg != mapChannelMessage.end(); ++itMapChannelMsg) {
        std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(itMapChannelMsg->first);
        if (ptrRedisServer != nullptr) {
            mapNodeCmds[ptrRedisServer].insert("PUBLISH " + itMapChannelMsg->first + " " + itMapChannelMsg->second);
        }
    }

    if (mapNodeCmds.empty()) {
        return false;
    }

    bool isSuccess = true;
    for (std::map<std::shared_ptr<RedisServer>, std::set<std::string>>::iterator itMapNodeKeys = mapNodeCmds.begin();
        itMapNodeKeys != mapNodeCmds.end(); ++itMapNodeKeys) {
        std::shared_ptr<RedisConn> pRedisConn = itMapNodeKeys->first->getRedisConn();
        if (pRedisConn != nullptr) {
            isSuccess &= pRedisConn->publishBatch(itMapNodeKeys->second);
            itMapNodeKeys->first->freeRedisConn(pRedisConn);
        }
    }

    return isSuccess;
}

uint32_t RedisClientSync::info_uptime()
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer("info");
    if (ptrRedisServer == nullptr) {
        return 0;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[INFO_UPTIME] failed to get available redis connection.";
        return 0;
    }

    uint32_t uUptime = pRedisConn->info_uptime();
    ptrRedisServer->freeRedisConn(pRedisConn);
    return uUptime;    
}

bool RedisClientSync::hsetnx(
    const std::string& key,
    const std::string& field,
    const std::string& value)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[HSETNX] failed to get available redis connection.";
        return false;
    }

    bool isSuccess = pRedisConn->redisCmdArgs3(
        std::string("HSETNX"),
        key.c_str(), key.size(),
        field.c_str(), field.size(),
        value.c_str(), value.size());
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::hlen(
    const std::string& key,
    uint64_t& num)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[HLEN] failed to get available redis connection.";
        return false;
    }

    bool isSuccess = pRedisConn->hlen(key, num);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::hscan(
    const std::string& key,
    const std::string& cursor,
    const std::string& pattern,
    uint32_t count,
    std::string& new_cursor,
    std::map<std::string, std::string>& results)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[HSCAN] failed to get available redis connection.";
        return false;
    }

    bool isSuccess = pRedisConn->hscan(key, cursor, pattern, count, new_cursor, results);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::hmset(
    const std::string& key,
    const std::vector<HField>& values)
{
    if (values.empty()) {
        LOGE << "[HMSET] no field specified.";
        return false;
    }

    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[HMSET] failed to get available redis connection.";
        return false;
    }

    std::string cmd = "HMSET " + key;
    for (auto& v : values) {
        cmd += " ";
        cmd += v.field;
        cmd += " ";
        cmd += v.value;
    }

    bool isSuccess = pRedisConn->unifiedCall(cmd);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::hscan(
        int32_t dbIndex,
        const std::string& key,
        const std::string& cursor,
        const std::string& pattern,
        uint32_t count,
        std::string& new_cursor,
        std::map<std::string, std::string>& results)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServerById(dbIndex);
    if (ptrRedisServer == nullptr) {
        LOGE << "[HSCAN] no redisServer db: " << dbIndex << ", key: " << key;
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[HSCAN] failed to get available redis connection. db: " << dbIndex << ", key: " << key;
        return false;
    }

    bool isSuccess = pRedisConn->hscan(key, cursor, pattern, count, new_cursor, results);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::hmset(
        int32_t dbIndex,
        const std::string& key,
        const std::vector<HField>& values)
{
    if (values.empty()) {
        LOGE << "[HMSET] no field specified db: " << dbIndex << ", key: " << key;
        return false;
    }

    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServerById(dbIndex);
    if (ptrRedisServer == nullptr) {
        LOGE << "[HMSET] no redisServer db: " << dbIndex << ", key: " << key;
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[HMSET] failed to get available redis connection db: " << dbIndex << ", key: " << key;
        return false;
    }

    bool isSuccess = pRedisConn->hmset(key, values);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::hmget(int32_t dbIndex, const std::string& key,
                            const std::vector<std::string>& fields,
                            std::map<std::string, std::string>& mapFieldValue)
{
    if (fields.empty()) {
        LOGE << "[HMGET] no field specified. db: " << dbIndex << ", key: " << key;
        return false;
    }

    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServerById(dbIndex);
    if (ptrRedisServer == nullptr) {
        LOGE << "[HMGET] no redisServer db: " << dbIndex << ", key: " << key;
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[HMGET] failed to get available redis connection. db: " << dbIndex << ", key: " << key;
        return false;
    }

    bool isSuccess = pRedisConn->hmget(key, fields, mapFieldValue);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::hdel(int32_t dbIndex, const std::string& key, const std::vector<std::string>& fieldList)
{
    if (fieldList.empty()) {
        LOGE << "[HDEL] no field specified. db: " << dbIndex << ", key: " << key;
        return false;
    }

    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServerById(dbIndex);
    if (ptrRedisServer == nullptr) {
        LOGE << "[HDEL] no redisServer db: " << dbIndex << ", key: " << key;
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[HDEL] failed to get available redis connection. db: " << dbIndex << ", key: " << key;
        return false;
    }

    bool isSuccess = pRedisConn->hdel(key, fieldList);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::zadd(int32_t dbIndex, const std::string& key,
          const std::string& mem, const int64_t& score)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServerById(dbIndex);
    if (ptrRedisServer == nullptr) {
        LOGE << "[ZADD] no redisServer db: " << dbIndex << ", key: " << key;
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[ZADD] failed to get available redis connection. db: " << dbIndex << ", key: " << key;
        return false;
    }

    bool isSuccess = pRedisConn->zadd(key, mem, score);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::zrem(int32_t dbIndex, const std::string& key,
          const std::vector<std::string>& memberList)
{
    if (memberList.empty()) {
        LOGE << "[ZREM] no field specified. db: " << dbIndex << ", key: " << key;
        return false;
    }

    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServerById(dbIndex);
    if (ptrRedisServer == nullptr) {
        LOGE << "[ZREM] no redisServer db: " << dbIndex << ", key: " << key;
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[ZREM] failed to get available redis connection. db: " << dbIndex << ", key: " << key;
        return false;
    }

    bool isSuccess = pRedisConn->zrem(key, memberList);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

bool RedisClientSync::getMemsByScoreWithLimit(int32_t dbIndex,
                             const std::string& key,
                             const int64_t min,
                             const int64_t max,
                             const uint32_t offset,
                             const uint32_t limit,
                             std::vector<ZSetMemberScore>& mems)
{
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServerById(dbIndex);
    if (ptrRedisServer == nullptr) {
        LOGE << "[ZRANGEBYSCORE] no redisServer db: " << dbIndex << ", key: " << key;
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[ZRANGEBYSCORE] failed to get available redis connection. db: " << dbIndex << ", key: " << key;
        return false;
    }

    bool isSuccess = pRedisConn->getMemsByScoreWithLimit(key, min, max, offset, limit, mems);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return isSuccess;
}

// Return
//     -1: communication error
//      0: key is not a valid integer
// Others: success
int32_t RedisClientSync::incr(const std::string& key, uint64_t& newValue) {
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[INCR] failed to get available redis connection.";
        return false;
    }

    int32_t ret = pRedisConn->incr(key, newValue);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return ret;
}

// Return
//     -1: communication error
//      0: key does not exist
// Others: timeout was set successfully
int32_t RedisClientSync::expire(const std::string& key, uint32_t timeout) {
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[EXPIRE] failed to get available redis connection.";
        return false;
    }

    int32_t ret = pRedisConn->expire(key, timeout);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return ret;
}

// Return
//     -1: communication error
//      0: key does not exist or has no associated expire
// Others: TTL in second
int64_t RedisClientSync::ttl(const std::string& key) {
    std::shared_ptr<RedisServer> ptrRedisServer = getRedisServer(key);
    if (ptrRedisServer == nullptr) {
        return false;
    }

    std::shared_ptr<RedisConn> pRedisConn = ptrRedisServer->getRedisConn();
    if (pRedisConn == nullptr) {
        LOGE << "[TTL] failed to get available redis connection.";
        return false;
    }

    int64_t ret = pRedisConn->ttl(key);
    ptrRedisServer->freeRedisConn(pRedisConn);
    return ret;
}

RedisClientAsync::RedisClientAsync()
{
}

RedisClientAsync::~RedisClientAsync()
{
    unsubscribe(m_ptrRedisServer->getKeepaliveKey());

    if (m_pLaterConnectEvent != nullptr) {
        event_free(m_pLaterConnectEvent);
        m_pLaterConnectEvent = nullptr;
    }
    
    if (m_pAsyncRedisConnect != nullptr) {
        redisAsyncDisconnect(m_pAsyncRedisConnect);
        redisAsyncFree(m_pAsyncRedisConnect);
        m_pAsyncRedisConnect = nullptr;
    }

    if (m_pAsyncEventBase != nullptr) {
        event_base_free(m_pAsyncEventBase);
        m_pAsyncEventBase = nullptr;
    }

    m_redisThread.join();
}

bool RedisClientAsync::run()
{
    LOGI << "redis client thread is started.";

    //create event base for main loop.
    m_pAsyncEventBase = event_base_new();

    if (createAsyncConn() == true) {
        asyncRedisLoop();
    }

    return true;
}

void RedisClientAsync::setAsyncHandler(IAsyncRedisEvent* pAsyncHandler)
{
    m_pAsyncAppServer = pAsyncHandler;
}

void RedisClientAsync::setRedisConfig(const std::vector<RedisConfig>& redisHosts)
{
    for (auto redisConfig : redisHosts) {
        m_ptrRedisServer = std::make_shared<RedisServer>(redisConfig.ip, redisConfig.port, redisConfig.password, redisConfig.regkey);
        LOGI << "parsed target async redis server.(" << redisConfig.ip << ":" << redisConfig.port << " " << redisConfig.password << ")";
        break;
    }
}

void RedisClientAsync::startAsyncRedisThread()
{
    m_redisThread = std::thread([&]() {
        setCurrentThreadName("redis.async");
        run();
    });
}

bool RedisClientAsync::asyncRedisLoop()
{
    signal(SIGPIPE, SIG_IGN);
    LOGI << "enter hiredis event loop..." << std::endl;
    try {
        struct event evTime;
        event_assign(&evTime, m_pAsyncEventBase, -1, EV_PERSIST, onTimeout5s, (void*)this);

        struct timeval tv;
        evutil_timerclear(&tv);
        tv.tv_sec = 5;
        event_add(&evTime, &tv);

        // base event loopback.
        event_base_dispatch(m_pAsyncEventBase);
    }
    catch (std::runtime_error err) {
        std::cout << err.what() << std::endl;
        return false;
    }

    return true;
}

void RedisClientAsync::onTimeout5s(evutil_socket_t, short, void *arg)
{
    RedisClientAsync* pthis = static_cast<RedisClientAsync*>(arg);
    if (pthis != nullptr) {
        //pthis->subscribe(pthis->m_ptrRedisServer->getKeepaliveKey());
    }
}

bool RedisClientAsync::createAsyncConn()
{
    //async connect to redis.    
    m_pAsyncRedisConnect = redisAsyncConnect(m_ptrRedisServer->getRedisHost().c_str(), m_ptrRedisServer->getRedisPort());
    if (m_pAsyncRedisConnect == nullptr) {
        return false;
    }

    //set private data.
    m_pAsyncRedisConnect->data = this;
    if (m_pAsyncRedisConnect->err) {
        LOGE << "create async redis connect error.(" << m_ptrRedisServer->getRedisHost() << ":" 
            << m_ptrRedisServer->getRedisPort() << " error:" << m_pAsyncRedisConnect->errstr << ")";
        return false;
    }

    if (redisLibeventAttach(m_pAsyncRedisConnect, m_pAsyncEventBase) != REDIS_OK) {
        LOGE << "failed to attach connect fd to epoll.(" << m_ptrRedisServer->getRedisHost() << ":" << m_ptrRedisServer->getRedisPort() << ")";
        return false;
    }

    //set connected callback.
    if (redisAsyncSetConnectCallback(m_pAsyncRedisConnect, onRedisAyncConnected) != 0) {
        LOGE << "failed to registe connected callback.(" << m_ptrRedisServer->getRedisHost() << ":" << m_ptrRedisServer->getRedisPort() << ")";
        return false;
    }

    //set disconnect callback.    
    if (redisAsyncSetDisconnectCallback(m_pAsyncRedisConnect, onRedisDisconnected) != 0)
    {
        LOGE << "failed to registe disconnect callback.";
        return false;
    }

    if (m_pLaterConnectEvent != nullptr) {
        event_free(m_pLaterConnectEvent);
    }
    m_pLaterConnectEvent = event_new(m_pAsyncEventBase, -1, 0, onReCreateAsyncConn, (void*)this);

    LOGI << "success to create async redis connection.(" << m_ptrRedisServer->getRedisHost() << ":" << m_ptrRedisServer->getRedisPort() << ")";
    
    return true;
}

bool RedisClientAsync::subscribe(const std::string& channel)
{
    if (!m_isAsyncConnected) {
        return false;
    }
    
    return asyncCommand("subscribe " + channel);
}

bool RedisClientAsync::unsubscribe(const std::string& channel)
{
    if (!m_isAsyncConnected) {
        return false;
    }
    
    return asyncCommand("unsubscribe " + channel);
}

bool RedisClientAsync::psubscribe(const std::string& channel)
{
    if (!m_isAsyncConnected) {
        return false;
    }

    return asyncCommand("psubscribe " + channel);
}

bool RedisClientAsync::asyncCommand(const std::string& strAsyncCmd)
{
    if (m_pAsyncRedisConnect == nullptr) {
        return false;
    }

    Task::asyncInvoke(m_pAsyncEventBase, [this, strAsyncCmd]() {
        redisAsyncCommand(m_pAsyncRedisConnect, onRedisMessage, (void*)this, strAsyncCmd.c_str());
    });

    //TODO: actual return will be implemented later.
    return true;

//    int ret = redisAsyncCommand(m_pAsyncRedisConnect, onRedisMessage, (void*)this, strAsyncCmd.c_str());
//    if (ret == REDIS_ERR) {
//        LOGE << "failed to post redis async cmd.(" << strAsyncCmd << ")";
//        return false;
//    }
//
//    LOGI << "success to post redis async cmd.(" << strAsyncCmd << ")";
//    return true;
}

void RedisClientAsync::doAuthRedis()
{
    if (m_pAsyncRedisConnect == nullptr) {
        LOGE << "async redis connection not ready.";
        return;
    }
    
    if (m_ptrRedisServer->getRedisPassword() == "") {
        LOGI << "not try redis authentication since no password provided.";
        return;
    }

    std::string strAsyncCmd = "AUTH " + m_ptrRedisServer->getRedisPassword();
    int ret = redisAsyncCommand(m_pAsyncRedisConnect, onAuthRes, (void*)this, strAsyncCmd.c_str());
    if (ret == REDIS_ERR) {
        LOGE << "failed to post redis auth cmd.(" << strAsyncCmd << ")";
        return;
    }

    LOGI << "success to post redis auth cmd.(" << strAsyncCmd << ")";
}

bool RedisClientAsync::connectRedisLater()
{
    struct timeval tv;
    evutil_timerclear(&tv);
    tv.tv_sec = kRedisConnectLater;

    if (event_add(m_pLaterConnectEvent, &tv) == -1) {
        LOGE << "failed to add later connect event.";
        return false;
    }

    return true;
}

void RedisClientAsync::onReCreateAsyncConn(evutil_socket_t, short, void *arg)
{
    LOGI << "try to recreate async redis connection.";

    RedisClientAsync* pthis = static_cast<RedisClientAsync*>(arg);
    if (pthis != nullptr) {
        pthis->createAsyncConn();
    }
}

void RedisClientAsync::onRedisMessage(redisAsyncContext* pRedisContext, void* pvRedisReply, void* pPrivateData)
{
    if ((pRedisContext == nullptr) || (pvRedisReply == nullptr) || (pPrivateData == nullptr)) {
        LOGE << "[onRedisMessage] receive redis message response but params exception.";
        return;
    }

    RedisClientAsync* pthis = static_cast<RedisClientAsync*>(pPrivateData);
    if (pthis->m_pAsyncAppServer == nullptr) {
        LOGE << "[onRedisMessage] async redis handler not ready.";
        return;
    }

    //check redis reply.
    redisReply* pRedisReply = static_cast<redisReply*>(pvRedisReply);
    if ((pRedisContext->err != 0) || (pRedisReply->type != REDIS_REPLY_ARRAY) || (pRedisReply->elements <= 0)) {
        LOGE << "[onRedisMessage] receive redis message exception from redis.(" 
            << pRedisReply->type << "," << pRedisReply->elements << " errno " 
            << pRedisContext->err << ":" << pRedisContext->errstr << ")";
        return;
    }

    //subscribe response.
    if (((strncmp(pRedisReply->element[0]->str, "psubscribe", 10) == 0) || 
        (strncmp(pRedisReply->element[0]->str, "subscribe", 9) == 0)) && (pRedisReply->elements >= 2)) {
        if (strncmp(pRedisReply->element[1]->str, "keepalive", 9) != 0) {
            LOGI << "[onRedisMessage] success to (" << pRedisReply->element[0]->str << " " << pRedisReply->element[1]->str<< ") from redis and notify dispatcher.";
            pthis->m_pAsyncAppServer->onRedisSubscribed(std::string(pRedisReply->element[1]->str));
        }
        return;
    }

    //unsubscribe response.
    if (((strncmp(pRedisReply->element[0]->str, "punsubscribe", 12) == 0) || 
        (strncmp(pRedisReply->element[0]->str, "unsubscribe", 11) == 0)) && (pRedisReply->elements >= 2)) {
        if (strncmp(pRedisReply->element[1]->str, "keepalive", 9) != 0) {
            LOGI << "[onRedisMessage] success to (" << pRedisReply->element[0]->str << " " << pRedisReply->element[1]->str << ") from redis and notify dispatcher.";
            //pthis->m_pAsyncAppServer->onUnsubscribed(std::string(pRedisReply->element[1]->str));
        }
        return;
    }

    //psubscribe message.
    if ((strncmp(pRedisReply->element[0]->str, "pmessage", 8) == 0) && (pRedisReply->elements == 4)) {
        pthis->m_pAsyncAppServer->onRedisMessage(std::string(pRedisReply->element[2]->str), std::string(pRedisReply->element[3]->str));
        LOGI << "[onRedisMessage] receive pmessage response from redis.(" << pRedisReply->element[2]->str << ")";
        return;
    }

    // subscribe message.
    if ((strncmp(pRedisReply->element[0]->str, "message", 7) == 0) && (pRedisReply->elements == 3)) {
        pthis->m_pAsyncAppServer->onRedisMessage(std::string(pRedisReply->element[1]->str), std::string(pRedisReply->element[2]->str, pRedisReply->element[2]->len));
        LOGI << "[onRedisMessage] receive message from redis.(" << pRedisReply->element[1]->str << " size:" << pRedisReply->element[2]->len << ")";
        return;
    }

    LOGE << "[onRedisMessage] receive exception response from redis.(type " << pRedisReply->type << " " << pRedisReply->elements << ")";

}

void RedisClientAsync::onRedisAyncConnected(const redisAsyncContext *pRedisContext, int status)
{
    if ((pRedisContext == nullptr) || (pRedisContext->data == nullptr)) {
        return;
    }

    RedisClientAsync* pthis = static_cast<RedisClientAsync*>(pRedisContext->data);
    
    //reconnect if failed.
    if (status != REDIS_OK) {
        LOGE << "failed to connect to redis and retry later.(error:" << pRedisContext->errstr << ")";
        pthis->connectRedisLater();
        return;
    }

    LOGI << "success to async connect to redis.";

    m_isAsyncConnected = true;

    //auth password.
    pthis->doAuthRedis();

    //notify application.
    if (pthis->m_pAsyncAppServer != nullptr) {
        pthis->m_pAsyncAppServer->onRedisAsyncConnected();
    }
}

void RedisClientAsync::onRedisDisconnected(const redisAsyncContext *pRedisContext, int status)
{
    m_isAsyncConnected = false;

    if (pRedisContext == nullptr) {
        return;
    }
    
    LOGW << "async redis connect disconnected.(status:" << status << " error:" << pRedisContext->errstr << ")";

    RedisClientAsync* pthis = static_cast<RedisClientAsync*>(pRedisContext->data);
    if (pthis != nullptr) {
        pthis->createAsyncConn();
    }
}

void RedisClientAsync::onAuthRes(redisAsyncContext* pRedisContext, void* pvRedisReply, void*)
{
    if ((pRedisContext == nullptr) || (pvRedisReply == nullptr)) {
        LOGE << "[onAuthRes] auth response callback params exception!";
        return;
    }

    redisReply* pRedisReply = static_cast<redisReply*>(pvRedisReply);
    if ((pRedisContext->err != 0) || (pRedisReply->type != REDIS_REPLY_STATUS ) || (pRedisReply->str == nullptr)) {
        LOGE << "[onAuthRes] auth exception from redis!(" << pRedisReply->type << " " << pRedisContext->err << ":" << pRedisContext->errstr << ")";
        return;
    }

    if (strncmp(pRedisReply->str, "OK", 2) == 0) {
        LOGI << "[onAuthRes] success auth response from redis.(" << pRedisReply->type << " " <<  pRedisReply->str << ")";
    }
    else {
        LOGE << "[onAuthRes] failed auth response from redis.(" << pRedisReply->type << " "<<  pRedisReply->str << ")";
    }    
}

RedisServer::RedisServer(const std::string& host, const int port, const std::string& password, const std::string& keepaliveKey)
    : m_redisHost(host)
    , m_redisPort(port)
    , m_redisPassword(password)
    , m_keepaliveKey(keepaliveKey)
{
}

std::shared_ptr<RedisConn> RedisServer::getRedisConn()
{
    //find a redis connect from pool.
    std::unique_lock<std::recursive_mutex> mutexRecu(m_mutexConnectList);
    std::shared_ptr<RedisConn> pRedisConn = nullptr;
    if (m_redisConnList.size() == 0) {
        pRedisConn = std::make_shared<RedisConn>(m_redisHost, m_redisPort, m_redisPassword);
        if (pRedisConn->connectRedis() == false) {
            pRedisConn = nullptr;
        }
    }
    else {
        pRedisConn = m_redisConnList.front();
        m_redisConnList.pop_front();
    }

    return std::move(pRedisConn);
} 

void RedisServer::freeRedisConn(std::shared_ptr<RedisConn> pRedisConn)
{
    std::unique_lock<std::recursive_mutex> mutexRecu(m_mutexConnectList);
    m_redisConnList.push_back(pRedisConn);
}

void RedisServer::syncRedisKeepAlive()
{
    std::unique_lock<std::recursive_mutex> mutexRecu(m_mutexConnectList);
    for (std::list<std::shared_ptr<RedisConn>>::iterator itRedisConn = m_redisConnList.begin();
        itRedisConn != m_redisConnList.end(); ++itRedisConn) {
        (*itRedisConn)->redisCmdArgs0(std::string("PING"));
    }   
}

RedisConn::RedisConn(const std::string& host, const int port, const std::string& password) :
    m_redisHost(host),
    m_redisPort(port),
    m_redisPassword(password),
    m_dwLastActiveTime(0),
    m_pRedisContext(nullptr)
{
}

RedisConn::~RedisConn()
{
    freeConnect();
}

bool RedisConn::connectRedis()
{
    if (m_pRedisContext != nullptr) {
        return true;
    }

    //redis connect
    struct timeval timeout = {1, 500000}; // 1.5 seconds
    m_pRedisContext = redisConnectWithTimeout(m_redisHost.c_str(), m_redisPort, timeout);
    if ((m_pRedisContext == nullptr) || (m_pRedisContext->err != REDIS_OK)) {
        if (m_pRedisContext != nullptr) {
            LOGE << "failed to connect redis (" << m_redisHost << ":" << m_redisPort << " error " << m_pRedisContext->errstr << ")";
            freeConnect();
        } else {
            LOGE << "failed to connect redis (can't allocate redis context)";
        }

        return false;
    }

    //update redis connect active time.
    m_dwLastActiveTime = nowInMilli();

    // redis auth if required.
    if (!m_redisPassword.empty()) {
        if (redisCmdArgs1(std::string("AUTH"), m_redisPassword.c_str(), m_redisPassword.size()) == false) {
            freeConnect();
            LOGE << "failed to auth redis (" << m_redisHost << ":" << m_redisPort << " error " << m_pRedisContext->errstr << ")";
            return false;
        }

        LOGI << "success to auth redis.(" << m_redisHost << ":" << m_redisPort  <<  " " << m_redisPassword << ")";
    }

    // enable tcp keep alive option (tcp_keepalive_time),
    if (redisEnableKeepAlive(m_pRedisContext) != REDIS_OK) {
        LOGW << "failed to enable tcp keepalive option.";
    }

    LOGI << "success to connect redis (" << m_redisHost << ":" << m_redisPort  <<  " " << m_redisPassword << ").";
    return true;
}

bool RedisConn::reConnectRedis()
{
    freeConnect();
    return connectRedis();
}

void RedisConn::freeConnect()
{
    if (m_pRedisContext != nullptr) {
        redisFree(m_pRedisContext);
        m_pRedisContext = nullptr;
        LOGI << "disconnect redis connect(" << m_redisHost << ":" << m_redisPort << ").";
    }
}

bool RedisConn::redisCmdArgs0(const std::string& strCmd)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }
        
    int i = 0;
    while (i++ < 2) {
        redisReply* pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, strCmd.c_str()));
        if (isReplySuccess(pReply) == true) {
            freeReplyObject(pReply);
            break;
        }

        LOGE << "[redisCmdArgs0] failed to excute (" << strCmd << " error: " << getReplyError(pReply) << ").";
        freeReplyObject(pReply);
        if ((i > 1) || (reConnectRedis() == false)) {
            return false;
        }
    }

    LOGI << "[redisCmdArgs0] success to excute (" << strCmd << ").";
    m_dwLastActiveTime = nowInMilli();
    return true;
}

bool RedisConn::redisCmdArgs1(const std::string& strCmd, const char* pchKey, int nKeySize)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }
        
    int i = 0;
    while (i++ < 2) {
        redisReply* pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, "%s %b", strCmd.c_str(), pchKey, nKeySize));
        if (isReplySuccess(pReply) == true) {
            freeReplyObject(pReply);
            break;
        }

        LOGE << "[redisCmdArgs1] failed to excute (" << strCmd << " error: " << getReplyError(pReply) << ").";
        freeReplyObject(pReply);
        if ((i > 1) || (reConnectRedis() == false)) {
            return false;
        }
    }

    LOGI << "[redisCmdArgs1] success to excute (" << strCmd << ").";
    m_dwLastActiveTime = nowInMilli();
    return true;
}

bool RedisConn::redisCmdArgs2(const std::string& strCmd, const char* pchKey, int nKeySize, const char* pchValue, int nValueSize)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }
        
    int i = 0;
    while (i++ < 2) {
        redisReply* pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, "%s %b %b", strCmd.c_str(), pchKey, nKeySize, pchValue, nValueSize));
        if (isReplySuccess(pReply) == true) {
            freeReplyObject(pReply);
            break;
        }

        LOGE << "[redisCmdArgs2] failed to excute (" << strCmd << " error: " << getReplyError(pReply) << ").";
        freeReplyObject(pReply);
        if ((i > 1) || (reConnectRedis() == false)) {
            return false;
        }
    }

    LOGI << "[redisCmdArgs2] success to excute (" << strCmd << ").";
    m_dwLastActiveTime = nowInMilli();
    return true;
}

bool RedisConn::redisCmdArgs3(const std::string& strCmd, const char* pchKey, int nKeySize, const char* pchField, int nFieldSize, const char* pchValue, int nValueSize)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }
        
    int i = 0;
    while (i++ < 2) {
        redisReply* pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, "%s %b %b %b", strCmd.c_str(), pchKey, nKeySize, pchField, nFieldSize, pchValue, nValueSize));
        if (isReplySuccess(pReply) == true) {
            freeReplyObject(pReply);
            break;
        }

        LOGE << "[redisCmdArgs3] failed to excute (" << strCmd << " error: " << getReplyError(pReply) << ").";
        freeReplyObject(pReply);
        if ((i > 1) || (reConnectRedis() == false)) {
            return false;
        }
    }

    LOGI << "[redisCmdArgs3] success to excute (" << strCmd << ").";
    m_dwLastActiveTime = nowInMilli();
    return true;
}

bool RedisConn::get(const std::string& strKey, std::string& strValue)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    int i = 0;
    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, "%s %b", "GET", strKey.c_str(), strKey.size()));
        if (isReplySuccess(pReply) == true) {
            break;
        }
        
        LOGE << "failed to excute (" << "GET " << strKey << " error: " << getReplyError(pReply) << ").";
        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return false;
        }
    }

    if (REDIS_REPLY_STRING == pReply->type) {
        strValue.assign(pReply->str, pReply->len);
    } else {
        if (REDIS_REPLY_NIL != pReply->type) {
            LOGE << "[GET] reply error type: " << getStringType(pReply) << ", key: " << strKey;
        }
    }
    m_dwLastActiveTime = nowInMilli();
    freeReplyObject(pReply);
    return true;
}

bool RedisConn::set(const std::string& key, const std::string& value, const int exptime)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }
        
    int i = 0;
    while (i++ < 2) {
        redisReply* pReply = nullptr;
        if (exptime != 0) {
            pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, "SET %b %b EX %d", key.c_str(), key.size(), value.c_str(), value.size(), exptime));
        } else {
            pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, "SET %b %b", key.c_str(), key.size(), value.c_str(), value.size()));
        }
        
        if (isReplySuccess(pReply) == true) {
            freeReplyObject(pReply);
            break;
        }

        LOGE << "[SET] failed to excute set command (" << " error: " << getReplyError(pReply) << ").";
        freeReplyObject(pReply);
        if ((i > 1) || (reConnectRedis() == false)) {
            return false;
        }
    }

    LOGI << "[SET] success to excute set command key: " << key << ", exptime: " << exptime ;
    m_dwLastActiveTime = nowInMilli();
    return true;
}


bool RedisConn::mget(const std::set<std::string>& setKeys, std::unordered_map<std::string, std::string>& mapKeyValues)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    std::ostringstream sstrKeys;
    sstrKeys << "mget ";
    for (std::set<std::string>::const_iterator cnitSetValue = setKeys.begin(); cnitSetValue != setKeys.end(); cnitSetValue++) {
        if (cnitSetValue != setKeys.begin()) {
            sstrKeys << " ";
        }
        sstrKeys << (*cnitSetValue);
    }

    int i = 0;
    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, sstrKeys.str().c_str()));
        if (isReplySuccess(pReply) == true) {
            break;
        }

        LOGE << "failed to excute mget (error: " << getReplyError(pReply) << ").";
        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return false;
        }
    }

    int nIndex = 0;
    std::set<std::string>::const_iterator cnitKey = setKeys.begin();
    for (; ((cnitKey != setKeys.end()) && (nIndex < static_cast<int>(pReply->elements))); cnitKey++, nIndex++) {
        if (pReply->element[nIndex]->str == nullptr) {
            mapKeyValues[*cnitKey] = "";
        }
        else {
            mapKeyValues[*cnitKey] = pReply->element[nIndex]->str;
        }
    }

    m_dwLastActiveTime = nowInMilli();
    freeReplyObject(pReply);
    return true;
}

bool RedisConn::hget(const std::string& key, const std::string& field, std::string& value)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    int i = 0;
    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, "HGET %b %b", key.c_str(), key.size(), field.c_str(), field.size()));
        if (isReplySuccess(pReply) == true) {
            break;
        }
        LOGE << "failed to excute hget, key: " << key
             << ", field: " << field
             << ", (error: " << getReplyError(pReply) << ")"
             << ", value: " << value;

        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return false;
        }
    }

    if (REDIS_REPLY_STRING == pReply->type) {
        value = pReply->str;
    } else {
        LOGI << "[HGET] reply type: " << getStringType(pReply);
    }

    m_dwLastActiveTime = nowInMilli();
    freeReplyObject(pReply);
    return true;
}

bool RedisConn::hmget(const std::string& key, const std::vector<std::string>& fields, std::map<std::string, std::string>& mapFieldValue)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    std::ostringstream sstrFields;

    sstrFields << "HMGET " << key;
    for (const auto& itField : fields) {
        sstrFields << " " << itField;
    }

    int i = 0;
    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, sstrFields.str().c_str()));
        if (isReplySuccess(pReply) == true) {
            break;
        }
        
        LOGE << "failed to excute hmget key: " << key
                << ", (error: " << getReplyError(pReply) << ")"
                << ", cmd: " << sstrFields.str();

        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return false;
        }
    }

    if (REDIS_REPLY_ARRAY == pReply->type) {
        for ( unsigned int j = 0; j < pReply->elements; ++j) {
            if (REDIS_REPLY_NIL != pReply->element[j]->type) {
                //field exists
                mapFieldValue[ fields[j] ] = pReply->element[j]->str;
            } else {
                mapFieldValue[ fields[j] ] = "";
            }
        }
    }

    m_dwLastActiveTime = nowInMilli();
    freeReplyObject(pReply);
    return true;
}

bool RedisConn::hmset(const std::string& key, const std::vector<HField>& values)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    std::vector<const char *> argv( values.size() *2 + 2 );
    std::vector<size_t> argvlen( values.size() *2 + 2 );

    uint32_t j = 0;
    static char msethash[] = "HMSET";
    argv[j] = msethash;
    argvlen[j] = sizeof(msethash)-1;

    j++;
    argv[j] = key.c_str();
    argvlen[j] = key.size();

    std::stringstream ss;
    for (const auto& it : values) {
        j++;
        argv[j] = it.field.data();
        argvlen[j] = it.field.size();

        j++;
        argv[j] = it.value.data();
        argvlen[j] = it.value.size();

        ss << "<" << it.field << "," << it.value << ">,";
    }

    int i = 0;
    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = (redisReply*)redisCommandArgv(m_pRedisContext, argv.size(), &(argv[0]), &(argvlen[0]));
        if (isReplySuccess(pReply) == true) {
            break;
        }

        LOGE << "failed to excute hmset key: " << key
             << ", (error: " << getReplyError(pReply) << ")"
             << ", cmd: " << ss.str();

        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return false;
        }
    }

    freeReplyObject(pReply);
    return true;
}


bool RedisConn::smembers(const std::string& key, std::vector<std::string>& memberList)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    int i = 0;
    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, "SMEMBERS %b", key.c_str(), key.size()));
        if (isReplySuccess(pReply) == true) {
            break;
        }

        LOGE << "failed to excute smembers (error: " << getReplyError(pReply) << ").";

        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return false;
        }
    }

    for (int i = 0; i < static_cast<int>(pReply->elements); i++) {
        if (pReply->element[i]->str == nullptr) {
            memberList.push_back("");
        } else {
            memberList.push_back(pReply->element[i]->str);
        }
    }

    m_dwLastActiveTime = nowInMilli();
    freeReplyObject(pReply);
    return true;
}

bool RedisConn::smembersBatch(const std::set<std::string>& setKeys, std::unordered_map<std::string, std::vector<std::string>>& mapKeyMembers)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    int nRetries = 0;
    int nFailures = 0;
    while (nRetries++ < 2) {
        //append cmds into pipeline output buffer.
        nFailures = 0;
        for (std::set<std::string>::const_iterator itKey = setKeys.begin(); itKey != setKeys.end(); ++itKey) {
            redisAppendCommand(m_pRedisContext, "SMEMBERS %b", itKey->c_str(), itKey->size());
        }

        //flush output buffer and handle reply.
        for (std::set<std::string>::const_iterator itKey = setKeys.begin(); itKey != setKeys.end(); ++itKey) {
            redisReply* pReply = nullptr;
            int nReply = redisGetReply(m_pRedisContext, (void **)&pReply);
            if ((nReply == REDIS_ERR) || (isReplySuccess(pReply) == false)) {
                std::string err_msg = "[smembersBatch] redisGetReply code: " + std::to_string(nReply);
                if (pReply != nullptr) {
                    err_msg = err_msg + ", reply type: " + getStringType(pReply);
                }

                LOGE << "[smembersBatch]" << err_msg;
                nFailures++;
                freeReplyObject(pReply);
                continue;
            }
            
            for (int i = 0; i < static_cast<int>(pReply->elements); i++) {
                if (pReply->element[i]->str == nullptr) {
                    mapKeyMembers[*itKey].push_back("");
                } else {
                    mapKeyMembers[*itKey].push_back(pReply->element[i]->str);
                }
            }
            
            freeReplyObject(pReply);
        }

        if ((nFailures == static_cast<int>(setKeys.size())) && (nRetries < 2) && (reConnectRedis() == true)) {
            continue;
        }

        return true;
    }

    return ((nFailures > 0) ? false : true);
}

bool RedisConn::pubsub(const std::string& strTopic, std::set<std::string>& setTopics)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    int i = 0;
    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, "pubsub channels %b", strTopic.c_str(), strTopic.size()));
        if (isReplySuccess(pReply) == true) {
            break;
        }

        LOGE << "failed to excute pubsub (error: " << getReplyError(pReply) << ").";
        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return false;
        }
    }
    
    for (int i = 0; i < static_cast<int>(pReply->elements); i++) {
        setTopics.insert(pReply->element[i]->str);
    }

    m_dwLastActiveTime = nowInMilli();
    freeReplyObject(pReply);
    return true;
}

bool RedisConn::publishRes(const std::string& channel, const std::string& message)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    int i = 0;
    int nSubscribe = 0;
    while (i++ < 2) {
        redisReply* pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, "PUBLISH %b %b", channel.c_str(), channel.size(), message.c_str(), message.size()));
        if (isReplySuccess(pReply) == true) {
            if (pReply->type == REDIS_REPLY_INTEGER) {
                nSubscribe = pReply->integer;
            } else {
                LOGI << "publish reply type: " << getStringType(pReply);
            }
            freeReplyObject(pReply);
            break;
        }

        LOGE << "[publishRes] failed to excute (" << channel << " error: " << getReplyError(pReply) << ").";
        freeReplyObject(pReply);
        if ((i > 1) || (reConnectRedis() == false)) {
            return false;
        }
    }

    LOGI << "[publishRes] success to excute (" << channel << " subscribers: " << nSubscribe << ").";
    m_dwLastActiveTime = nowInMilli();
    return (nSubscribe > 0);
}

bool RedisConn::publishBatch(std::set<std::string>& setCmds)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    int nRetries = 0;
    int nFailures = 0;
    while (nRetries++ < 2) {
        //append publish cmds into pipeline output buffer.
        nFailures = 0;
        for (std::set<std::string>::iterator itSetCmd = setCmds.begin(); itSetCmd != setCmds.end(); ++itSetCmd) {
            redisAppendCommand(m_pRedisContext, "%s", itSetCmd->c_str());//TODO:justinfang - not support binary key value.
        }

        //flush output buffer and handle reply.
        for (size_t i = 0; i < setCmds.size(); i++) {
            redisReply* pReply = nullptr;
            int nReply = redisGetReply(m_pRedisContext, (void **)&pReply);
            if ((nReply == REDIS_ERR) || (isReplySuccess(pReply) == false)) {
                std::string err_msg = "[publishBatch] redisGetReply code: " + std::to_string(nReply);
                if (pReply != nullptr) {
                    err_msg = err_msg + ", reply type: " + getStringType(pReply);
                }
                LOGE << err_msg;
                nFailures++;
            }
        
            freeReplyObject(pReply);
        }

        if ((nFailures == static_cast<int>(setCmds.size())) && (nRetries < 2) && (reConnectRedis() == true)) {
            continue;
        }

        return true;
    }

    return ((nFailures > 0) ? false : true);
}

uint32_t RedisConn::info_uptime()
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return 0;
        }
    }

    int i = 0;
    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, "info"));
        if (isReplySuccess(pReply) == true) {
            break;
        }

        LOGE << "[info_uptime] failed to info redis! error: " << getReplyError(pReply);
        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return 0;
        }
    }

    std::vector<std::string> vecWords;
    boost::split(vecWords, pReply->str, boost::is_any_of( ":\r\n" ));
    if((vecWords.size() < 40) || (vecWords[38] != "uptime_in_seconds")) {
        return 0;
    }
    
    m_dwLastActiveTime = nowInMilli();
    freeReplyObject(pReply);
    std::string::size_type sz;
    return std::stoi (vecWords[39], &sz);
}

bool RedisConn::isReplySuccess(const redisReply* pReply)
 {
    if (nullptr == pReply) {
        return false;
    }

    switch (pReply->type) {
        case REDIS_REPLY_STRING:
        case REDIS_REPLY_ARRAY:
        case REDIS_REPLY_INTEGER:
        case REDIS_REPLY_STATUS:

            // The command replied with a nil object. There is no data to access.
        case REDIS_REPLY_NIL:
            return true;

        case REDIS_REPLY_ERROR:
        default:
            return false;
    }

    return false;
}

std::string RedisConn::getReplyError(const redisReply* reply)
{
    std::string err = "Undefined";
    if (nullptr == reply) {
        return err;
    }

    if (REDIS_REPLY_ERROR == reply->type  ||
        REDIS_REPLY_STATUS == reply->type ||
        REDIS_REPLY_STRING == reply->type) {
        err = std::string(reply->str, reply->len);
    }

    return err;
}

std::string RedisConn::getStringType(const redisReply* reply)
{
    std::string type = "Unknown";
    if (nullptr == reply) {
        return type;
    }

    switch (reply->type) {
        case REDIS_REPLY_STRING:
            type = "String";
            break;

        case REDIS_REPLY_ARRAY:
            type = "Array";
            break;

        case REDIS_REPLY_INTEGER:
            type = "Integer";
            break;

        case REDIS_REPLY_STATUS:
            type = "Status";
            break;

        case REDIS_REPLY_NIL:
            type = "Nil";
            break;

        case REDIS_REPLY_ERROR:
            type = "Error";
            break;

        default:
            break;
    }

    return type;
}

// Add for decoupling online and offline modules
bool RedisConn::unifiedCall(
    const std::string& fullCmd)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    int i = 0;
    while (i++ < 2) {
        redisReply* pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, fullCmd.c_str()));
        if (isReplySuccess(pReply) == true) {
            freeReplyObject(pReply);
            break;
        }

        LOGE << "[unifiedCall] failed to excute (" << fullCmd
            << ", error: " << getReplyError(pReply) << ").";
        freeReplyObject(pReply);
        if ((i > 1) || (reConnectRedis() == false)) {
            return false;
        }
    }

    LOGI << "[unifiedCall] success to excute (" << fullCmd << ").";
    m_dwLastActiveTime = nowInMilli();
    return true;
}

bool RedisConn::hlen(
    const std::string& strKey,
    uint64_t& num)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    num = 0;
    int i = 0;
    const std::string strCmd = "HLEN " + strKey;

    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, strCmd.c_str()));
        if (isReplySuccess(pReply) == true) {
            break;
        }

        LOGE << "failed to excute (" << strCmd << " error: " << getReplyError(pReply) << ").";
        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return false;
        }
    }

    LOGI << "success to excute (" << strCmd <<  ")";
    if (REDIS_REPLY_INTEGER == pReply->type) {
        num = pReply->integer;
    } else {
        LOGE << "unexpected [HLEN] reply type: " << getStringType(pReply) << ".";
    }

    m_dwLastActiveTime = nowInMilli();
    freeReplyObject(pReply);
    return true;
}

bool RedisConn::hscan(
    const std::string& key,
    const std::string& cursor,
    const std::string& pattern,
    uint32_t count,
    std::string& new_cursor,
    std::map<std::string, std::string>& results)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    // Caller wants to call like an append style ?
    new_cursor = "";
    results.clear();
    int i = 0;
    std::string cmd = "HSCAN " + key + " " + cursor;
    if (!pattern.empty()) {
        cmd += " MATCH " + pattern;
    }
    if (count > 0) {
        cmd += " COUNT " + std::to_string(count);
    }

    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, cmd.c_str()));
        if (isReplySuccess(pReply) == true) {
            break;
        }

        LOGE << "failed to excute (" << cmd << " error: " << getReplyError(pReply) << ").";
        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return false;
        }
    }

    LOGI << "success to excute (" << cmd <<  ")";
    // Return value is an array of two values: the first value is the new cursor
    // to use in the next call, the second value is an array of elements.
    if (REDIS_REPLY_ARRAY == pReply->type) {
        redisReply* c = pReply->element[0];
        redisReply* r = pReply->element[1];
        assert(REDIS_REPLY_STRING == c->type && REDIS_REPLY_ARRAY == r->type);

        new_cursor = c->str;

        std::string tmpKey = "";
        for (size_t n = 0; n < r->elements; n++) {
            assert(REDIS_REPLY_STRING == r->element[n]->type);

            if (n % 2 == 0) {
                tmpKey = r->element[n]->str;
            } else {
                results[tmpKey] = r->element[n]->str;
            }
        }
    } else {
        LOGE << "unexpected [HSCAN] reply type: " << getStringType(pReply) << ".";
    }

    m_dwLastActiveTime = nowInMilli();
    freeReplyObject(pReply);
    return true;
}

bool RedisConn::hdel(const std::string& key, const std::vector<std::string>& fieldList)
{
    if (fieldList.empty()) {
        return false;
    }

    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    std::vector<const char *> argv( fieldList.size() + 2 );
    std::vector<size_t> argvlen( fieldList.size() + 2 );

    uint32_t j = 0;
    static char zRemCmd[] = "HDEL";
    argv[j] = zRemCmd;
    argvlen[j] = sizeof(zRemCmd)-1;

    j++;
    argv[j] = key.c_str();
    argvlen[j] = key.size();

    for (const auto& it : fieldList)
    {
        j++;
        argv[j] = it.data();
        argvlen[j] = it.size();
    }

    int i = 0;
    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = (redisReply*)redisCommandArgv(m_pRedisContext, argv.size(), &(argv[0]), &(argvlen[0]) );
        if (isReplySuccess(pReply) == true) {
            break;
        }
        LOGE << "failed to excute hdel key: " << key
             << ", (error: " << getReplyError(pReply) << ")"
             << ", field: " << toString(fieldList);

        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return false;
        }
    }

    freeReplyObject(pReply);
    return true;
}

bool RedisConn::zadd(
        const std::string& key,
        const std::string& mem,                     //ASCII字符串
        const int64_t& score)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    std::ostringstream ssCmdStr;
    ssCmdStr << "ZADD %b " << score << " %b";

    int i = 0;
    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, ssCmdStr.str().c_str(),
                                                       key.c_str(), key.size(), mem.c_str(), mem.size()));
        if (isReplySuccess(pReply) == true) {
            break;
        }

        LOGE << "failed to excute zadd key: " << key
             << ", (error: " << getReplyError(pReply) << ")"
             << ", mem: " << mem << ", score: " << score;

        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return false;
        }
    }

    freeReplyObject(pReply);
    return true;
}

bool RedisConn::zrem(const std::string& key, const std::vector<std::string>& memberList)
{
    if (memberList.empty()) {
        return false;
    }

    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    std::vector<const char *> argv( memberList.size() + 2 );
    std::vector<size_t> argvlen( memberList.size() + 2 );

    uint32_t j = 0;
    static char zRemCmd[] = "ZREM";
    argv[j] = zRemCmd;
    argvlen[j] = sizeof(zRemCmd)-1;

    j++;
    argv[j] = key.c_str();
    argvlen[j] = key.size();

    for (const auto& it : memberList)
    {
        j++;
        argv[j] = it.data();
        argvlen[j] = it.size();
    }

    int i = 0;
    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = (redisReply*)redisCommandArgv(m_pRedisContext, argv.size(), &(argv[0]), &(argvlen[0]));
        if (isReplySuccess(pReply) == true) {
            break;
        }

        LOGE << "failed to excute zrem key: " << key
             << ", (error: " << getReplyError(pReply) << ")"
             << ", field: " << toString(memberList);

        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return false;
        }
    }

    freeReplyObject(pReply);
    return true;
}


bool RedisConn::getMemsByScoreWithLimit(
        const std::string& key,
        const int64_t min,
        const int64_t max,
        const uint32_t offset,
        const uint32_t limit,
        std::vector<ZSetMemberScore>& mems)
{
    if (min > max)
    {
        return false;
    }

    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }

    std::string minStr = std::to_string(min);
    std::string maxStr = std::to_string(max);

    if (min == REDIS_NEGATIVE_INFINITY)
    {
        minStr = REDIS_NEGATIVE_INFINITY_STR;
    }

    if (max == REDIS_POSITIVE_INFINITY)
    {
        maxStr = REDIS_POSITIVE_INFINITY_STR;
    }

    std::ostringstream cmdStr;
    cmdStr << "ZRANGEBYSCORE " << key << " " << minStr << " " << maxStr << " WITHSCORES LIMIT " << offset << " " << limit;

    int i = 0;
    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, cmdStr.str().c_str()));
        if (isReplySuccess(pReply) == true) {
            break;
        }

        LOGE << "failed to excute getMemsByScoreWithLimit key: " << key
             << ", (error: " << getReplyError(pReply) << ")"
             << ", cmd: " << cmdStr.str();

        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return false;
        }
    }

    if (REDIS_REPLY_ARRAY == pReply->type) {
        if (pReply->elements > 0) {
            //result format: uid1 num1 uid2 num2 uid3 num3...
            for ( uint32_t i = 0; i < pReply->elements - 1; ) {
                ZSetMemberScore data;
                data.member = pReply->element[i++]->str;
                data.score  = std::stoll(pReply->element[i++]->str);

                mems.emplace_back(data);
            }
        }
    }

    freeReplyObject(pReply);
    return true;
}


// Return
//     -1: communication error
//      0: key is not a valid integer
// Others: success
int32_t RedisConn::incr(const std::string& key, uint64_t& newValue)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return -1;
        }
    }

    int i = 0;
    std::string strCmd = "INCR " + key;
    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, strCmd.c_str()));
        if (isReplySuccess(pReply) == true) {
            break;
        }
        
        LOGE << "failed to excute: " << strCmd
             << ", retry: " << i
             << ", type: " << pReply->type
             << ", integer: " << pReply->integer
             << ", redis context err: " << m_pRedisContext->err
             << ", error: " << getReplyError(pReply);

        freeReplyObject(pReply);
    
        if (REDIS_OK == m_pRedisContext->err) {
            return 0;
        }
        
        if ((i >= 2) || (reConnectRedis() == false)) {
            return -1;
        }
    }

    LOGI << "success to excute (" << strCmd <<  ")";
    int32_t ret = -1;
    if (REDIS_REPLY_INTEGER == pReply->type) {
        newValue = pReply->integer;
        ret = 1;
    } else {
        LOGE << "unexpected [INCR] reply type: " << getStringType(pReply) << ".";
        ret = 0;
    }

    m_dwLastActiveTime = nowInMilli();
    freeReplyObject(pReply);
    return ret;
}

// Return
//     -1: communication error
//      0: key does not exist
// Others: timeout was set successfully
int32_t RedisConn::expire(const std::string& key, uint32_t timeout)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return -1;
        }
    }

    int i = 0;
    std::string strCmd = "EXPIRE " + key + " " + std::to_string(timeout);
    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, strCmd.c_str()));
        if (isReplySuccess(pReply) == true) {
            break;
        }

        LOGE << "failed to excute (" << strCmd << " error: " << getReplyError(pReply) << ").";
        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return -1;
        }
    }

    LOGI << "success to excute (" << strCmd <<  ")";
    int32_t ret = -1;
    if (REDIS_REPLY_INTEGER == pReply->type) {
        // Integer reply, specifically:
        //     1 if the timeout was set.
        //     0 if key does not exist.
        ret = pReply->integer;
    } else {
        LOGE << "unexpected [EXPIRE] reply type: " << getStringType(pReply) << ".";
        ret = -1;
    }

    m_dwLastActiveTime = nowInMilli();
    freeReplyObject(pReply);
    return ret;
}

// Return
//     -1: communication error
//      0: key does not exist or has no associated expire
// Others: TTL in second
int64_t RedisConn::ttl(const std::string& key) {
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return -1;
        }
    }

    int i = 0;
    std::string strCmd = "TTL " + key;
    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, strCmd.c_str()));
        if (isReplySuccess(pReply) == true) {
            break;
        }

        LOGE << "failed to excute (" << strCmd << " error: " << getReplyError(pReply) << ").";
        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return -1;
        }
    }

    LOGI << "success to excute (" << strCmd <<  ")";
    int64_t ret = -1;
    if (REDIS_REPLY_INTEGER == pReply->type) {
        // Starting with Redis 2.8 the return value in case of error changed:
        // - The command returns -2 if the key does not exist.
        // - The command returns -1 if the key exists but has no associated expire.
        ret = pReply->integer;
        if (-2 == ret || -1 == ret) {
            ret = 0;
        }
    } else {
        LOGE << "unexpected [TTL] reply type: " << getStringType(pReply) << ".";
        ret = -1;
    }

    m_dwLastActiveTime = nowInMilli();
    freeReplyObject(pReply);
    return ret;
}

bool RedisConn::del(const std::string& key)
{
    if (m_pRedisContext == nullptr) {
        if (reConnectRedis() == false) {
            return false;
        }
    }
    
    int i = 0;
    std::string strCmd = "DEL " + key;
    redisReply* pReply = nullptr;
    while (i++ < 2) {
        pReply = static_cast<redisReply*>(redisCommand(m_pRedisContext, strCmd.c_str()));
        if (isReplySuccess(pReply) == true) {
            break;
        }
        
        LOGE << "failed to excute (" << strCmd << " error: " << getReplyError(pReply) << ").";
        freeReplyObject(pReply);
        if ((i >= 2) || (reConnectRedis() == false)) {
            return false;
        }
    }

    freeReplyObject(pReply);
    return true;
}