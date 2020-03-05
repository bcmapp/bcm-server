#include <shared_mutex>

#include "group_msg_sub.h"
#include "redis/online_redis_manager.h"
#include "redis/reply.h"
#include "utils/log.h"

#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/thread.h>
#include <hiredis/hiredis.h>
#include <boost/thread/barrier.hpp>

#include "proto/dao/group_msg.pb.h"

namespace bcm {

// -----------------------------------------------------------------------------
// Section: GroupMsgSubImpl
// -----------------------------------------------------------------------------
class GroupMsgSubImpl : public redis::AsyncConn::ISubscriptionHandler {
    typedef GroupMsgSub::IMessageHandler IMessageHandler;

    std::vector<IMessageHandler*> m_msgHandlers;

public:
    GroupMsgSubImpl()
    {
        OnlineRedisManager::Instance()->subscribe("group_event_msg", this);
    }

    virtual ~GroupMsgSubImpl()
    {
    }

    void addMessageHandler(IMessageHandler* handler)
    {
        if (std::find(m_msgHandlers.begin(), m_msgHandlers.end(), handler) 
                == m_msgHandlers.end()) {
            m_msgHandlers.emplace_back(handler);
        }
    }

    void removeMessageHandler(IMessageHandler* handler)
    {
        std::vector<IMessageHandler*>::iterator it = 
            std::find(m_msgHandlers.begin(), m_msgHandlers.end(), handler);
        if (it != m_msgHandlers.end()) {
            m_msgHandlers.erase(it);
        }
    }

    void subscribeGids(const std::vector<uint64_t>& gids)
    {
        if (gids.empty()) {
            return;
        }

        for (auto& g : gids) {
            OnlineRedisManager::Instance()->subscribe("group_" + std::to_string(g), this);
        }
    }

    void unsubcribeGids(const std::vector<uint64_t>& gids)
    {
        if (gids.empty()) {
            return;
        }

        for (auto& g : gids) {
            OnlineRedisManager::Instance()->unsubscribe("group_" + std::to_string(g));
        }
    }

private:
    void onSubscribe(const std::string& chan) override
    {
        LOGT << "subscribed to channel " << chan;
    }

    void onUnsubscribe(const std::string& chan) override
    {
        LOGT << "unsubscribed from channel " << chan;
    }

    void onMessage(const std::string& chan, const std::string& msg) override
    {
        if (GroupMsgSub::isGroupMessageChannel(chan)) {
            for (IMessageHandler* h : m_msgHandlers) {
                h->handleMessage(chan, msg);
            }
        }
    }

    void onError(int code) override
    {
        LOGE << "redis error: " << code;
    }
};

// -----------------------------------------------------------------------------
// Section: GroupMsgSub
// -----------------------------------------------------------------------------
GroupMsgSub::GroupMsgSub()
    : m_pImpl(new GroupMsgSubImpl())
    , m_impl(*m_pImpl)
{
}

GroupMsgSub::~GroupMsgSub()
{
    if (m_pImpl != nullptr) {
        delete m_pImpl;
    }
}

void GroupMsgSub::addMessageHandler(IMessageHandler* handler)
{
    m_impl.addMessageHandler(handler);
}

void GroupMsgSub::removeMessageHandler(IMessageHandler* handler)
{
    m_impl.removeMessageHandler(handler);
}

void GroupMsgSub::subscribeGids(const std::vector<uint64_t>& gids)
{
    m_impl.subscribeGids(gids);
}
void GroupMsgSub::unsubcribeGids(const std::vector<uint64_t>& gids)
{
    m_impl.unsubcribeGids(gids);
}

// static
bool GroupMsgSub::isGroupMessageChannel(const std::string& chan)
{
    return (chan.find("group_") == 0) || (chan.find("instant_") == 0);
}

} // namespace bcm