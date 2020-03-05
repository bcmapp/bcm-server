#pragma once

#include <thread>
#include <functional>
#include <vector>

struct event_base;

namespace bcm {
class GroupMsgSubImpl;

class GroupMsgSub {
public:
    GroupMsgSub();
    ~GroupMsgSub();

public:
    struct IMessageHandler {
        virtual ~IMessageHandler() {}
        virtual void handleMessage(const std::string& chan, 
            const std::string& msg) = 0;
    };

    void addMessageHandler(IMessageHandler* handler);
    void removeMessageHandler(IMessageHandler* handler);

    static bool isGroupMessageChannel(const std::string& chan);

    void subscribeGids(const std::vector<uint64_t>& gids);
    void unsubcribeGids(const std::vector<uint64_t>& gids);

private:
    GroupMsgSubImpl* m_pImpl;
    GroupMsgSubImpl& m_impl;
};

} // namespace bcm
