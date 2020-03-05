#pragma once

#include <utils/jsonable.h>

namespace bcm {

struct NoiseConfig {
    bool enabled = false;
    double percentage = 0.0;
    uint64_t iosSupportedVersion = 0;
    uint64_t androidSupportedVersion = 0;
};

inline void to_json(nlohmann::json& j, const NoiseConfig& config)
{
    j = nlohmann::json{{
                               "enabled",                   config.enabled
                       },
                       {
                               "percentage",                config.percentage
                       },
                       {
                               "iosSupportedVersion",     config.iosSupportedVersion
                       },
                       {
                               "androidSupportedVersion", config.androidSupportedVersion
                       }};
}

inline void from_json(const nlohmann::json& j, NoiseConfig& config)
{
    jsonable::toBoolean(j, "enabled", config.enabled);
    jsonable::toNumber(j, "percentage", config.percentage);
    jsonable::toNumber(j, "iosSupportedVersion", config.iosSupportedVersion);
    jsonable::toNumber(j, "androidSupportedVersion", config.androidSupportedVersion);
}
}
