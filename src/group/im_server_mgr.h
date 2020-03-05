#pragma once

#include <string>
#include <vector>
#include <shared_mutex>

#include <event2/event_struct.h>

#include "redis/async_conn.h"
#include "../utils/consistent_hash.h"

namespace bcm {
struct RedisConfig;
} // namespace bcm

namespace bcm {

class ImServerMgr {
public:
    typedef redis::AsyncConn::DisconnectHandler DisconnectHandler;

    ImServerMgr(struct event_base* eb, const RedisConfig& redisCfg);

    void addRegKey(const std::string& key);

    // WARNNING: *DO NOT* call this function
    void onTick();

    void shutdown(DisconnectHandler&& handler);
    bool shouldHandleGroup(uint64_t gid);
    std::string getServerByGroup(uint64_t gid);
    std::string getServerRandomly() const;
    bool isSelf(const std::string& addr) const;

    struct IServerListUpdateListener {
        virtual void onServerListUpdate() = 0;
    };
    void setServerListUpdateListener(IServerListUpdateListener* listener);

private:
    void onConnect(int status);
    void pingRedis();
    void updateImServerList();

private:
    struct event m_evtTick;
    redis::AsyncConn m_redisConn;
    std::vector<std::string> m_imsvrs;
    mutable std::shared_timed_mutex m_imsvrsMtx;
    UserConsistentFnvHash m_consistentHash;
    mutable std::shared_timed_mutex m_constHashMtx;
    IServerListUpdateListener* m_listener;
    std::vector<std::string> m_regKeys;
};

} // namespace bcm