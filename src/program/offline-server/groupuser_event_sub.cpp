#include "groupuser_event_sub.h"

#include <shared_mutex>

#include "redis/async_conn.h"
#include "redis/reply.h"
#include "utils/log.h"

#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/thread.h>
#include <hiredis/hiredis.h>
#include <boost/thread/barrier.hpp>


namespace bcm {

static const int kRedisKeepAliveIntervalSecs = 30;

// -----------------------------------------------------------------------------
// Section: GroupMsgSubImpl
// -----------------------------------------------------------------------------
class GroupUserEventSubImpl : public redis::AsyncConn::ISubscriptionHandler {
    typedef GroupUserEventSub::ShutdownHandler ShutdownHandler;
    typedef GroupUserEventSub::IMessageHandler IMessageHandler;

    struct event_base* m_eb;
    struct event m_evtKeepAlive;
    redis::AsyncConn m_conn;
    std::vector<IMessageHandler*> m_msgHandlers;

    std::set<uint64_t>  m_subscribeChanSet;
    std::shared_timed_mutex m_subChanMtx;

public:
    GroupUserEventSubImpl(struct event_base* eb,
                    const std::string& host, int port,
                    const std::string& password)
        : m_eb(eb), m_conn(eb, host, port, password)
    {
        event_assign(&m_evtKeepAlive, m_eb, -1, EV_READ | EV_PERSIST,
                     handleKeepAlive, (void*)this);
        struct timeval tv;
        evutil_timerclear(&tv);
        tv.tv_sec = kRedisKeepAliveIntervalSecs;
        event_add(&m_evtKeepAlive, &tv);

        m_conn.setOnReconnectHandler(std::bind(&GroupUserEventSubImpl::onReconnect,
                                               this, std::placeholders::_1));
        m_conn.start(std::bind(&GroupUserEventSubImpl::onConnect, this,
                               std::placeholders::_1));
    }

    virtual ~GroupUserEventSubImpl()
    {
        clearSubcirbeGids();
    }

    void shutdown(ShutdownHandler&& handler)
    {
        event_del(&m_evtKeepAlive);
        m_conn.shutdown(std::forward<ShutdownHandler>(handler));
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

        std::vector<std::string>  vecChan;
        {
            std::unique_lock<std::shared_timed_mutex> l(m_subChanMtx);
            for (const auto& it : gids) {
                vecChan.push_back("groupEvent_" + std::to_string(it));
                m_subscribeChanSet.insert(it);
            }
        }

        m_conn.psubscribeBatch(vecChan, this);
    }

    void unsubcribeGids(const std::vector<uint64_t>& gids)
    {
        if (gids.empty()) {
            return;
        }

        std::vector<std::string>  vecChan;

        {
            std::unique_lock<std::shared_timed_mutex> l(m_subChanMtx);
            for (const auto& it : gids) {
                vecChan.push_back("groupEvent_" + std::to_string(it));

                m_subscribeChanSet.erase(it);
            }
        }
        
        m_conn.punsubscribeBatch(vecChan);
    }

    void clearSubcirbeGids()
    {
        std::vector<uint64_t> gids;
        {
            std::unique_lock<std::shared_timed_mutex> l(m_subChanMtx);
            for (const auto& it : m_subscribeChanSet) {
                gids.push_back(it);
            }
        }

        unsubcribeGids(gids);
    }


private:
    void onConnect(int status)
    {
        if (REDIS_OK == status) {
            subscribeChannels();
        } else {
            LOGE << "connect error: " << status;
        }
    }

    void onReconnect(int status)
    {
        if (REDIS_OK == status) {
            subscribeChannels();
        } else {
            LOGE << "reconnect error: " << status;
        }
    }

    void subscribeChannels()
    {
        std::vector<uint64_t> gids;
        {
            std::unique_lock<std::shared_timed_mutex> l(m_subChanMtx);
            for (const auto& it : m_subscribeChanSet) {
                gids.push_back(it);
            }
        }

        if (!gids.empty()) {
            subscribeGids(gids);
        }

        m_conn.psubscribe("user_*", this);
    }

    void onSubscribe(const std::string& chan) override
    {
        LOGT << "subscribed to channel: " << chan;
    }

    void onUnsubscribe(const std::string& chan) override
    {
        LOGT << "unsubscribed from channel: " << chan;
    }

    void onMessage(const std::string& chan, const std::string& msg) override
    {
        for (IMessageHandler* h : m_msgHandlers) {
            h->handleMessage(chan, msg);
        }
    }

    void onError(int code) override
    {
        LOGE << "redis error: " << code;
    }

    static void handleKeepAlive(evutil_socket_t fd, short events, void *arg)
    {
        boost::ignore_unused(fd, events, arg);
        GroupUserEventSubImpl* pThis = reinterpret_cast<GroupUserEventSubImpl*>(arg);
        pThis->onKeepAlive();
    }

    void onKeepAlive()
    {
        m_conn.unsubscribe("group_event_sub:keepalive");
    }
};

// -----------------------------------------------------------------------------
// Section: GroupMsgSub
// -----------------------------------------------------------------------------
GroupUserEventSub::GroupUserEventSub(struct event_base* eb,
                         const std::string& host, int port,
                         const std::string& password)
    : m_pImpl(new GroupUserEventSubImpl(eb, host, port, password))
      , m_impl(*m_pImpl)
{
}

GroupUserEventSub::~GroupUserEventSub()
{
    if (m_pImpl != nullptr) {
        delete m_pImpl;
    }
}

void GroupUserEventSub::shutdown(ShutdownHandler&& handler)
{
    m_impl.clearSubcirbeGids();

    m_impl.shutdown(std::forward<ShutdownHandler>(handler));
}

void GroupUserEventSub::addMessageHandler(IMessageHandler* handler)
{
    m_impl.addMessageHandler(handler);
}

void GroupUserEventSub::removeMessageHandler(IMessageHandler* handler)
{
    m_impl.removeMessageHandler(handler);
}

void GroupUserEventSub::subscribeGids(const std::vector<uint64_t>& gids)
{
    m_impl.subscribeGids(gids);
}
void GroupUserEventSub::unsubcribeGids(const std::vector<uint64_t>& gids)
{
    m_impl.unsubcribeGids(gids);
}

} // namespace bcm