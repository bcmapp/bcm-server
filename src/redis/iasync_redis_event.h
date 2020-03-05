#pragma once
#include <string>

namespace bcm{

class IAsyncRedisEvent {
public:
    virtual void onRedisAsyncConnected() = 0;
    virtual void onRedisMessage(const std::string& serializedAddress, const std::string& serializedMessage) = 0;
    virtual void onRedisSubscribed(const std::string& serializedAddress) = 0;
};

}
