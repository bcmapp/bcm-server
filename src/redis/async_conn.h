#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>

struct event_base;

namespace bcm {
namespace redis {

class Reply;
class AsyncConnImpl;
// -----------------------------------------------------------------------------
// Section: AsyncConn
// -----------------------------------------------------------------------------
//
// NOTE: Caller is responsible for keeping event loop running until this 
// connection is completely shutdown (which means shutdown function is called 
// and shutdown complete handler is invoked)
//
// NOTE: All handlers will be called on the thread which runs event loop
//
class AsyncConn {
public:
    typedef std::function<void(int)> ConnectHandler;
    typedef std::function<void(int)> DisconnectHandler;
    typedef std::function<void(int, const Reply&)> ReplyHandler;

    AsyncConn(struct event_base* eb, const std::string& host, 
              int16_t port = 6379, const std::string& password = "");

    virtual ~AsyncConn() {}
    
    AsyncConn& setReconnectDelay(int millisecs);

    // Set a handler to be called when reconnected to redis server
    AsyncConn& setOnReconnectHandler(ConnectHandler&& handler);

    // Set a handler to be called when disconected from redis server
    AsyncConn& setOnDisconnectHandler(DisconnectHandler&& handler);

    void start(ConnectHandler&& handler);
    void shutdown(DisconnectHandler&& handler);
    void exec(ReplyHandler&& handler, const char* fmt, ...);

    struct ISubscriptionHandler {
        virtual ~ISubscriptionHandler() { }
        virtual void onSubscribe(const std::string& chan) = 0;
        virtual void onUnsubscribe(const std::string& chan) = 0;
        virtual void onMessage(const std::string& chan, 
                               const std::string& msg) = 0;
        virtual void onError(int code) = 0;
    };
    void subscribe(const std::string& chan, ISubscriptionHandler* handler);
    void psubscribe(const std::string& chan, ISubscriptionHandler* handler);
    void unsubscribe(const std::string& chan);
    void punsubscribe(const std::string& chan);

    void psubscribeBatch(const std::vector<std::string>& chanVec,
                                    ISubscriptionHandler* handler);
    void punsubscribeBatch(const std::vector<std::string>& chanVec);
    bool isSubcribeChannel(const std::string& chan);

    std::string getRedisHost();
    int32_t getRedisPort();


private:
    std::shared_ptr<AsyncConnImpl> m_pImpl;
};

} // namespace redis
} // namespace bcm