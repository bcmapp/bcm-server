#pragma once

#include <utils/jsonable.h>

namespace bcm {

struct LimiterConfig {
    int64_t configUpdateInterval{30 * 1000};

    LimiterConfig() = default;
    LimiterConfig(int64_t millis)
        : configUpdateInterval(millis)
    {
    }
};

inline void to_json(nlohmann::json& j, const LimiterConfig& config)
{
    j = nlohmann::json{{"configUpdateInterval", config.configUpdateInterval}};
}

inline void from_json(const nlohmann::json& j, LimiterConfig& config)
{
    jsonable::toNumber(j, "configUpdateInterval", config.configUpdateInterval);
}

}
