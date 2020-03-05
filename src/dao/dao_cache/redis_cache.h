#pragma once
#include "redis/redis_manager.h"

namespace bcm {
namespace dao {

class RedisCache {
public:
    RedisCache(int64_t ttl) : m_ttl(ttl) {}
    bool set(const std::string& key, const std::string& value);
    bool get(const std::string& key, std::string& value);

private:
    int64_t m_ttl;
};

}
}
