#include "group_event_sub.h"
#include "message_type.h"
#include "online_msg_member_mgr.h"
#include "group_event.h"
#include "config/redis_config.h"
#include "utils/log.h"

#include <boost/core/ignore_unused.hpp>
#include <hiredis/hiredis.h>
#include <nlohmann/json.hpp>

namespace bcm {

GroupEventSub::GroupEventSub(struct event_base* eb, 
                             const RedisConfig& redisCfg, 
                             OnlineMsgMemberMgr& onlineMsgMemberMgr)
    : m_eb(eb)
    , m_connSub(eb, redisCfg.ip, redisCfg.port, redisCfg.password)
    , m_onlineMsgMemberMgr(onlineMsgMemberMgr)
{
    m_connSub.setOnReconnectHandler(
        std::bind(&GroupEventSub::onRedisSubConnect, this, 
                  std::placeholders::_1));
    m_connSub.start(std::bind(&GroupEventSub::onRedisSubConnect, this, 
                              std::placeholders::_1));
}

void GroupEventSub::onRedisSubConnect(int status)
{
    boost::ignore_unused(status);
    m_connSub.psubscribe("user_*", this);
}

void GroupEventSub::onSubscribe(const std::string& chan)
{
    LOGT << "subscribed to channel " << chan;
}

void GroupEventSub::onUnsubscribe(const std::string& chan)
{
    LOGT << "unsubscribed from channel " << chan;
}

void GroupEventSub::onMessage(const std::string& chan, 
                                  const std::string& msg)
{
    boost::ignore_unused(chan);
    try {
        nlohmann::json msgObj = nlohmann::json::parse(msg);
        GroupEvent evt;
        msgObj.get_to(evt);
        handleEvent(evt.type, evt.uid, evt.gid);
    } catch (std::exception& e) {
        LOGE << "exception caught: " << e.what();
    }
}

void GroupEventSub::onError(int code)
{
    LOGE << "redis error: " << code;
}

void GroupEventSub::handleEvent(int type, const std::string& uid, 
                                    uint64_t gid)
{
    switch (type) {
    case INTERNAL_USER_ENTER_GROUP:
        m_onlineMsgMemberMgr.handleUserEnterGroup(uid, gid);
        break;
    case INTERNAL_USER_QUIT_GROUP:
        m_onlineMsgMemberMgr.handleUserLeaveGroup(uid, gid);
        break;
    case INTERNAL_USER_MUTE_GROUP:
        break;
    case INTERNAL_USER_UNMUTE_GROUP:
        break;
    }
}

void GroupEventSub::stop(std::function<void(bool)>&& handler)
{
    m_connSub.shutdown([h(std::move(handler))](int status) mutable {
        h(REDIS_OK == status);
    });
}

} // namespace bcm
