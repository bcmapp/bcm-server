#pragma once

#include <string>
#include <set>

#include <nlohmann/json.hpp>

#include "redis/async_conn.h"

struct event_base;

namespace bcm {
struct RedisConfig;
class OnlineMsgMemberMgr;
class OfflineMsgMemberMgr;
} // namespace bcm

namespace bcm {

class GroupEventSub : public redis::AsyncConn::ISubscriptionHandler {
    typedef std::set<std::string> UidSet;

public:
    GroupEventSub(struct event_base* eb, 
                  const RedisConfig& redisCfg, 
                  OnlineMsgMemberMgr& onlineMsgMemberMgr);

    void stop(std::function<void(bool)>&& handler);

private:
    void onRedisSubConnect(int status);

    // redis::AsyncConn::ISubscriptionHandler
    void onSubscribe(const std::string& chan) override;
    void onUnsubscribe(const std::string& chan) override;
    void onMessage(const std::string& chan, const std::string& msg) override;
    void onError(int code) override;

    void handleEvent(int type, const std::string& uid, uint64_t gid);

private:
    struct event_base* m_eb;
    redis::AsyncConn m_connSub;
    OnlineMsgMemberMgr& m_onlineMsgMemberMgr;
    UidSet m_onlineUidSet;
};

} // namespace bcm
