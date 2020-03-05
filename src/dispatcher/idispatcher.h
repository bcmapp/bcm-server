#pragma once
#include <string>

namespace bcm {

class IDispatcher {
public:
    virtual uint64_t getIdentity() = 0;

    virtual void onDispatchSubscribed() = 0;
    virtual void onDispatchUnsubscribed(bool kicking) = 0;
    virtual void onDispatchRedisMessage(const std::string& message) = 0;
    virtual void onDispatchGroupMessage(const std::string& message) = 0;
};

}
