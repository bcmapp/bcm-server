#pragma once

#include <utils/jsonable.h>

namespace bcm {

struct ThrottlingControlConfig {
    size_t groupCreationLimitation = 64;
    size_t groupCreationThrottlingInterval = 86400;
};

inline void to_json(nlohmann::json& j, const ThrottlingControlConfig& config)
{
    j = nlohmann::json{
        {"groupCreationLimitation", config.groupCreationLimitation},
        {"groupCreationThrottlingInterval", config.groupCreationThrottlingInterval}
        };
}

inline void from_json(const nlohmann::json& j, ThrottlingControlConfig& config)
{
    jsonable::toNumber(j, "groupCreationLimitation", config.groupCreationLimitation);
    jsonable::toNumber(j, "groupCreationThrottlingInterval", config.groupCreationThrottlingInterval);
}

}
