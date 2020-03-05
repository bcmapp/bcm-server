#include "redis_cache.h"

namespace bcm {
namespace dao {

bool RedisCache::set(const std::string& key, const std::string& value)
{
    return bcm::RedisDbManager::Instance()->set(key, key, value, m_ttl);
}

bool RedisCache::get(const std::string& key, std::string& value)
{
    return bcm::RedisDbManager::Instance()->get(key, key, value);
}

}
}
