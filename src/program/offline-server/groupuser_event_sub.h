#pragma once

#include <thread>
#include <functional>
#include <vector>

struct event_base;

namespace bcm {
class GroupUserEventSubImpl;

class GroupUserEventSub {
public:
    GroupUserEventSub(struct event_base* eb, const std::string& host, int port = 6379,
                const std::string& password = "");
    ~GroupUserEventSub();

public:
    typedef std::function<void(int)> ShutdownHandler;
    void shutdown(ShutdownHandler&& handler);

    struct IMessageHandler {
        virtual ~IMessageHandler() {}
        virtual void handleMessage(const std::string& chan,
                                   const std::string& msg) = 0;
    };

    void addMessageHandler(IMessageHandler* handler);
    void removeMessageHandler(IMessageHandler* handler);

    void subscribeGids(const std::vector<uint64_t>& gids);
    void unsubcribeGids(const std::vector<uint64_t>& gids);

private:
    GroupUserEventSubImpl* m_pImpl;
    GroupUserEventSubImpl& m_impl;
};

} // namespace bcm
