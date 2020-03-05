#include "async_conn.h"
#include "reply.h"
#include "utils/libevent_utils.h"
#include "utils/log.h"

#include <event2/event_struct.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>

#include <vector>

namespace bcm {
namespace redis {
// -----------------------------------------------------------------------------
// Section: ConnectionHandler
// -----------------------------------------------------------------------------
class ConnectionHandler {
public:
    virtual ~ConnectionHandler() {}

    void registerCallbacks(redisAsyncContext* ac)
    {
        ac->data = reinterpret_cast<void*>(this);
        redisAsyncSetConnectCallback(ac, handleConnect);
        redisAsyncSetDisconnectCallback(ac, handleDisconnect);  
    }

    virtual void onConnect(int status) = 0;
    virtual void onDisconnect(int status) = 0;

private:
    static void handleConnect(const redisAsyncContext* ac, int status)
    {
        ConnectionHandler* h = reinterpret_cast<ConnectionHandler*>(ac->data);
        h->onConnect(status);
    }

    static void handleDisconnect(const redisAsyncContext* ac, int status)
    {
        ConnectionHandler* h = reinterpret_cast<ConnectionHandler*>(ac->data);
        h->onDisconnect(status);
    }
};

// -----------------------------------------------------------------------------
// Section: AsyncCmd
// -----------------------------------------------------------------------------
class AsyncCmd : public libevent::AsyncTask {
    typedef AsyncConn::ReplyHandler ReplyHandler;

    std::shared_ptr<AsyncConnImpl> m_conn;
    char* m_cmd;
    std::size_t m_cmdLen;
    ReplyHandler m_handler;
    bool m_handlerInvoked;

private:
    static void onComplete(struct redisAsyncContext* ac, void* r, void* priv);
    void run() override;

public:
    explicit AsyncCmd(std::shared_ptr<AsyncConnImpl> conn);
    virtual ~AsyncCmd();

    AsyncCmd* setReplyHandler(ReplyHandler&& handler);
    bool execute(const char* fmt, va_list ap);
};

// -----------------------------------------------------------------------------
// Section: AsyncConnImpl
// -----------------------------------------------------------------------------
static const int kDefaultReconnectDelay = 500;

class AsyncConnImpl 
    : public std::enable_shared_from_this<AsyncConnImpl>
    , public ConnectionHandler {

    typedef AsyncConn::ConnectHandler ConnectHandler;
    typedef AsyncConn::DisconnectHandler DisconnectHandler;
    typedef AsyncConn::ReplyHandler ReplyHandler;
    typedef AsyncConn::ISubscriptionHandler ISubscriptionHandler;

    friend class AsyncCmd;

    struct redisAsyncContext*   m_ac;
    struct event_base*          m_eb;
    std::string                 m_host;
    int16_t                     m_port;
    std::string                 m_password;
    int                         m_reconnectDelayMilliSecs;
    libevent::AsyncFunc*        m_reconnectTask;
    ConnectHandler              m_reconnectHandler;
    DisconnectHandler           m_disconnectHandler;
    std::vector<ConnectHandler> m_connectHandlers;
    std::vector<DisconnectHandler> m_shutdownHandlers;

    enum ConnectionState {
        DISCONNECTED = 1,
        CONNECTING,
        CONNECTED,
        RECONNECTING,
        DISCONNECTING,
    };
    ConnectionState m_connectionState;

    typedef std::map<std::string, ISubscriptionHandler*> 
        SubscriptionHandlerMap;
    SubscriptionHandlerMap m_subHandlerMap;

public:
    AsyncConnImpl(struct event_base* eb, const std::string& host, int16_t port, 
                  const std::string& password)
        : m_eb(eb), m_host(host), m_port(port), m_password(password)
        , m_reconnectDelayMilliSecs(kDefaultReconnectDelay)
        , m_reconnectTask(nullptr)
        , m_connectionState(DISCONNECTED) {}

    virtual ~AsyncConnImpl() {}

    void setReconnectDelay(int millisecs)
    {
        m_reconnectDelayMilliSecs = millisecs;
    }

    void setOnReconnectHandler(ConnectHandler&& handler)
    {
        m_reconnectHandler = std::move(handler);
    }

    void setOnDisconnectHandler(DisconnectHandler&& handler)
    {
        m_disconnectHandler = std::move(handler);
    }

    void start(ConnectHandler&& handler)
    {
        auto self = shared_from_this();
        auto fn = [self, h(std::move(handler))]() mutable {
            self->startConnect(std::move(h));
        };
        libevent::AsyncFunc::invoke(m_eb, fn);
    }

    void shutdown(DisconnectHandler&& handler)
    {
        auto self = shared_from_this();
        auto fn = [self, h(std::move(handler))]() mutable {
            self->startDisconnect(std::move(h));
        };
        libevent::AsyncFunc::invoke(m_eb, fn);
    }

    void exec(ReplyHandler&& handler, const char* fmt, va_list ap)
    {
        (new AsyncCmd(shared_from_this()))
            ->setReplyHandler(std::forward<ReplyHandler>(handler))
            ->execute(fmt, ap);
    }

    void exec(ReplyHandler&& handler, const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        exec(std::forward<ReplyHandler>(handler), fmt, ap);
        va_end(ap);
    }

    void subscribe(const std::string& chan, 
                   AsyncConn::ISubscriptionHandler* handler)
    {
        libevent::AsyncFunc::invoke(m_eb, [=]() {
            int res = redisAsyncCommand(m_ac, handleSubscription, 
                                        reinterpret_cast<void*>(this), 
                                        "SUBSCRIBE %s", chan.c_str());
            if (REDIS_OK != res) {
                LOGE << "exec 'SUBSCRIBE " << chan << "' error: " << res;
                handler->onError(res);
            } else {
                m_subHandlerMap[chan] = handler;
            }
        });
    }

    void psubscribe(const std::string& chan, 
                    AsyncConn::ISubscriptionHandler* handler)
    {
        libevent::AsyncFunc::invoke(m_eb, [=]() {
            int res = redisAsyncCommand(m_ac, handleSubscription, 
                                        reinterpret_cast<void*>(this), 
                                        "PSUBSCRIBE %s", chan.c_str());
            if (REDIS_OK != res) {
                LOGE << "exec 'PSUBSCRIBE " << chan << "' error: " << res;
                handler->onError(res);
            } else {
                m_subHandlerMap[chan] = handler;
            }
        });
    }

    void unsubscribe(const std::string& chan)
    {
        libevent::AsyncFunc::invoke(m_eb, [=]() {
            int res = redisAsyncCommand(m_ac, handleSubscription, 
                                        reinterpret_cast<void*>(this), 
                                        "UNSUBSCRIBE %s", chan.c_str());
            if (REDIS_OK != res) {
                LOGE << "exec 'UNSUBSCRIBE " << chan << "' error: " << res;
                auto it = m_subHandlerMap.find(chan);
                if (it != m_subHandlerMap.end()) {
                    it->second->onError(res);
                }
            };
        });
    }

    void punsubscribe(const std::string& chan)
    {
        libevent::AsyncFunc::invoke(m_eb, [=]() {
            int res = redisAsyncCommand(m_ac, handleSubscription, 
                                        reinterpret_cast<void*>(this), 
                                        "PUNSUBSCRIBE %s", chan.c_str());
            if (REDIS_OK != res) {
                LOGE << "exec 'PUNSUBSCRIBE " << chan << "' error: " << res;
                auto it = m_subHandlerMap.find(chan);
                if (it != m_subHandlerMap.end()) {
                    it->second->onError(res);
                }
            };
        });
    }

    void psubscribeBatch(const std::vector<std::string>& chanVec,
                         AsyncConn::ISubscriptionHandler* handler)
    {
        if (chanVec.empty()) {
            return;
        }

        std::string strChannel = "PSUBSCRIBE";

        for (const auto& it : chanVec) {
            strChannel = strChannel + " " + it;
        }

        libevent::AsyncFunc::invoke(m_eb, [=]() {
            int res = redisAsyncCommand(m_ac, handleSubscription,
                                        reinterpret_cast<void*>(this),
                                        strChannel.c_str());
            if (REDIS_OK != res) {
                LOGE << "exec failed 'PSUBSCRIBE " << strChannel << "' error: " << res;
                handler->onError(res);
            } else {
                LOGI << "exec " << strChannel << " OK ";
                for(const auto& it : chanVec) {
                    m_subHandlerMap[it] = handler;
                }
            }
        });
    }

    void punsubscribeBatch(const std::vector<std::string>& chanVec)
    {
        if (chanVec.empty()) {
            return;
        }

        std::string strChannel = "PUNSUBSCRIBE";

        for (const auto& it : chanVec) {
            strChannel = strChannel + " " + it;
        }

        libevent::AsyncFunc::invoke(m_eb, [=]() {
            int res = redisAsyncCommand(m_ac, handleSubscription,
                                        reinterpret_cast<void*>(this),
                                        strChannel.c_str());
            if (REDIS_OK != res) {
                LOGE << "exec failed 'UNSUBSCRIBE " << strChannel << "' error: " << res;
            } else {
                LOGI << "exec " << strChannel << "  OK" ;
            };
        });
    }

    bool isSubcribeChannel(const std::string& chan) const
    {
        auto it = m_subHandlerMap.find(chan);
        if (it != m_subHandlerMap.end()) {
            return true;
        }
        return false;
    }

    std::string getRedisHost()
    {
        return m_host;
    }

    int32_t getRedisPort()
    {
        return m_port;
    }

private:
    void startConnect(ConnectHandler&& handler)
    {
        if (DISCONNECTED == m_connectionState) {
            m_connectionState = CONNECTING;
            m_connectHandlers.emplace_back(std::move(handler));
            startAsyncConnect();
        } else if (CONNECTING == m_connectionState) {
            m_connectHandlers.emplace_back(std::move(handler));
        } else {
            handler(REDIS_ERR_OTHER);
        }
    }

    void startDisconnect(DisconnectHandler&& handler)
    {
        if (m_reconnectTask != nullptr) {
            delete m_reconnectTask;
            m_reconnectTask = nullptr;
        }
        if (CONNECTED == m_connectionState) {
            m_connectionState = DISCONNECTING;
            m_shutdownHandlers.emplace_back(std::move(handler));
            redisAsyncDisconnect(m_ac);
        } else if (DISCONNECTING == m_connectionState) {
            m_shutdownHandlers.emplace_back(std::move(handler));
        } else if (m_ac != nullptr) {
            redisAsyncDisconnect(m_ac);
            m_connectionState = DISCONNECTED;
            m_ac = nullptr;
            handler(REDIS_OK);
        } else {
            m_connectionState = DISCONNECTED;
            handler(REDIS_OK);
        }
    }

    void startReconnect()
    {   
        if (m_reconnectTask != nullptr) {
            return;
        }
        LOGI << "reconnect redis server(" << m_host << ":" << m_port << ")" 
             << " in " << m_reconnectDelayMilliSecs << " milliseconds";
        auto self = shared_from_this();
        auto fn = [self]() { 
            self->startAsyncConnect(); 

            // |m_reconnectTask| will be deleted by AsyncFunc after this 
            // lamda returns
            self->m_reconnectTask = nullptr;
        };
        m_reconnectTask = new libevent::AsyncFunc(m_eb, fn);
        m_reconnectTask->executeWithDelay(m_reconnectDelayMilliSecs);
    }

    void startAsyncConnect()
    {
        LOGI << "connect redis server(" << m_host << ":" << m_port << ")";
        m_ac = redisAsyncConnect(m_host.c_str(), m_port);
        redisLibeventAttach(m_ac, m_eb);
        registerCallbacks(m_ac);
    }

    void onConnect(int status) override
    {
        if (status != REDIS_OK) {
            LOGE << "connect error with status: " << status;
            startReconnect();
            return;
        }
        LOGI << "redis server(" << m_host << ":" << m_port << ") connected" 
             << std::endl;
        ConnectionState prevStat = m_connectionState;
        m_connectionState = CONNECTED;
        if (!m_password.empty()) {
            exec(std::bind(&AsyncConnImpl::onAuthed, this, 
                    std::placeholders::_1, std::placeholders::_2,
                    RECONNECTING == prevStat),
                 "AUTH %b", m_password.c_str(), m_password.size());
            return;
        }
        if (CONNECTING == prevStat) {
            for (auto& h : m_connectHandlers) {
                h(status);
            }
            m_connectHandlers.clear();
        } else if (RECONNECTING == prevStat) {
            if (m_reconnectHandler != nullptr) {
                m_reconnectHandler(status);
            }
        }
    }

    void onAuthed(int status, const Reply& reply, bool isReconnect)
    {
        if (status != REDIS_OK) {
            // failed to execute command which may caused by loss of 
            // connnection, try again
            startReconnect();
            return;
        }
        if (reply.isError()) {
            LOGE << "auth error: " << reply.getError() << ", disconnect redis";
            status = REDIS_ERR;

            // wrong password, disconect to redis server
            startDisconnect([](int status) {
                LOGI << "redis server disconnected with status: " << status;
            });
        }
        if (isReconnect) {
            if (m_reconnectHandler != nullptr) {
                m_reconnectHandler(status);
            }
        } else {
            for (auto& h : m_connectHandlers) {
                h(status);
            }
            m_connectHandlers.clear();
        }
    }

    void onDisconnect(int status) override
    {
        LOGW << "redis server(" << m_host << ":" << m_port 
             << ") disconnected with status " << status;
        ConnectionState prevState = m_connectionState;
        m_connectionState = DISCONNECTED;
        m_ac = nullptr;
        if ( (status != REDIS_OK) && (DISCONNECTING != prevState) ) {
            if (m_disconnectHandler != nullptr) {
                m_disconnectHandler(status);
            }
            m_connectionState = RECONNECTING;
            startReconnect();
            return;
        }
        for (auto& h : m_shutdownHandlers) {
            h(status);
        }
        m_shutdownHandlers.clear();
    }

    static void handleSubscription(struct redisAsyncContext* ac, 
                                   void* r, void* priv)
    {
        boost::ignore_unused(ac);
        Reply reply = Reply(reinterpret_cast<redisReply*>(r));
        AsyncConnImpl* pThis = reinterpret_cast<AsyncConnImpl*>(priv);
        LOGT << "received subscription message: " << reply;

        if (reply.isSubscriptionReply()) {
            std::string chan = reply[1].getString();
            auto it = pThis->m_subHandlerMap.find(chan);
            if (it != pThis->m_subHandlerMap.end()) {
                it->second->onSubscribe(chan);
            }
            return;
        }

        if (reply.isUnsubscribeReply()) {
            std::string chan = reply[1].getString();
            auto it = pThis->m_subHandlerMap.find(chan);
            if (it != pThis->m_subHandlerMap.end()) {
                it->second->onUnsubscribe(chan);
                pThis->m_subHandlerMap.erase(it);
            }
            return;
        }

        if (reply.isSubscriptionMessage()) {
            std::string chan = reply[1].getString();
            auto it = pThis->m_subHandlerMap.find(chan);
            if (it != pThis->m_subHandlerMap.end()) {
                if (reply.isSubscribeMessage()) {
                    it->second->onMessage(chan, reply[2].getString());
                } else {
                    it->second->onMessage(reply[2].getString(), 
                                          reply[3].getString());
                }
            }
            return;
        }

        if (!reply.isNull()) {
            LOGE << "received unexpected redis message";
        }
    }
};

// -----------------------------------------------------------------------------
// Section: AsyncCmd implementation
// -----------------------------------------------------------------------------
AsyncCmd::AsyncCmd(std::shared_ptr<AsyncConnImpl> conn) 
    : libevent::AsyncTask(conn->m_eb), m_conn(conn), m_cmd(nullptr), m_cmdLen(0)
    , m_handlerInvoked(false)
{
    LOGT << "AsyncCmd created: " << this;
}

AsyncCmd::~AsyncCmd()
{
    LOGT << "AsyncCmd destroyed: " << this;
    if (m_cmd != nullptr) {
        free(m_cmd);
    }
}

AsyncCmd* AsyncCmd::setReplyHandler(ReplyHandler&& handler)
{
    m_handler = std::move(handler);
    return this;
}

bool AsyncCmd::execute(const char* fmt, va_list ap)
{
    int len = redisvFormatCommand(&m_cmd, fmt, ap);
    if (len < 0) {
        return false;
    }
    m_cmdLen = len;
    AsyncTask::execute();
    return true;
}

// static
void AsyncCmd::onComplete(struct redisAsyncContext* ac, void* r, void* priv)
{
    boost::ignore_unused(ac);
    Reply reply = Reply(reinterpret_cast<redisReply*>(r));
    AsyncCmd* pThis = reinterpret_cast<AsyncCmd*>(priv);

    LOGT << "received reply from redis: " << reply;

    if (!pThis->m_handlerInvoked && (pThis->m_handler != nullptr) ) {
        pThis->m_handlerInvoked = true;
        pThis->m_handler(REDIS_OK, reply);
    }

    delete pThis;
}

void AsyncCmd::run()
{
    if (AsyncConnImpl::CONNECTED != m_conn->m_connectionState) {
        if (m_handler != nullptr) {
            m_handler(REDIS_ERR, Reply());
        }
        delete this;
        return;
    }
    std::string cmd(m_cmd, m_cmdLen);
    boost::replace_all(cmd, "\r\n", " ");
    LOGT << "execute redis command: " << std::move(cmd);
    int res = redisAsyncFormattedCommand(m_conn->m_ac, onComplete, 
        reinterpret_cast<void*>(this), m_cmd, m_cmdLen);
    if (res != REDIS_OK) {
        if (m_handler != nullptr) {
            LOGE << "execute command error: " << res << ", cmd: " << m_cmd;
            m_handler(res, Reply());
        }
        delete this;
    }
}

// -----------------------------------------------------------------------------
// Section: AsyncConn
// -----------------------------------------------------------------------------
AsyncConn::AsyncConn(struct event_base* eb, const std::string& host, 
    int16_t port, const std::string& password)
    : m_pImpl(std::make_shared<AsyncConnImpl>(eb, host, port, password))
{
}

AsyncConn& AsyncConn::setReconnectDelay(int millisecs)
{
    m_pImpl->setReconnectDelay(millisecs);
    return *this;
}

AsyncConn& AsyncConn::setOnReconnectHandler(ConnectHandler&& handler)
{
    m_pImpl->setOnReconnectHandler(std::forward<ConnectHandler>(handler));
    return *this;
}

AsyncConn& AsyncConn::setOnDisconnectHandler(DisconnectHandler&& handler)
{
    m_pImpl->setOnDisconnectHandler(std::forward<DisconnectHandler>(handler));
    return *this;
}

void AsyncConn::start(ConnectHandler&& handler)
{
    m_pImpl->start(std::forward<ConnectHandler>(handler));
}
    
void AsyncConn::shutdown(DisconnectHandler&& handler)
{
    m_pImpl->shutdown(std::forward<DisconnectHandler>(handler));
}
    
void AsyncConn::exec(ReplyHandler&& handler, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    m_pImpl->exec(std::forward<ReplyHandler>(handler), fmt, ap);
    va_end(ap);
}

void AsyncConn::subscribe(const std::string& chan, 
                          ISubscriptionHandler* handler)
{
    m_pImpl->subscribe(chan, handler);
}

void AsyncConn::psubscribe(const std::string& chan, 
                           ISubscriptionHandler* handler)
{
    m_pImpl->psubscribe(chan, handler);
}

void AsyncConn::unsubscribe(const std::string& chan)
{
    m_pImpl->unsubscribe(chan);
}

void AsyncConn::punsubscribe(const std::string& chan)
{
    m_pImpl->punsubscribe(chan);
}

void AsyncConn::psubscribeBatch(const std::vector<std::string>& chanVec,
                           ISubscriptionHandler* handler)
{
    m_pImpl->psubscribeBatch(chanVec, handler);
}

void AsyncConn::punsubscribeBatch(const std::vector<std::string>& chanVec)
{
    m_pImpl->punsubscribeBatch(chanVec);
}

bool AsyncConn::isSubcribeChannel(const std::string& chan)
{
    return m_pImpl->isSubcribeChannel(chan);
}

std::string AsyncConn::getRedisHost()
{
    return m_pImpl->getRedisHost();
}

int32_t AsyncConn::getRedisPort()
{
    return m_pImpl->getRedisPort();
}

} // namespace redis
} // namespace bcm