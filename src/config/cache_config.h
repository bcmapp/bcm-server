#pragma once

#include <utils/jsonable.h>

namespace bcm {

struct CacheConfig {
    uint64_t groupKeysLimit = 2 << 31;
};

inline void to_json(nlohmann::json& j, const CacheConfig& config)
{
    j = nlohmann::json{{
                               "groupKeysLimit",                   config.groupKeysLimit
                       }};
}

inline void from_json(const nlohmann::json& j, CacheConfig& config)
{
    jsonable::toNumber(j, "groupKeysLimit", config.groupKeysLimit);
}
}
