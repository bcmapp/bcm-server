#pragma once

#include <utils/jsonable.h>

namespace bcm {

struct SysMsgConfig {
    bool openSysMsgService = false;
};

inline void to_json(nlohmann::json& j, const SysMsgConfig config)
{
    j = nlohmann::json{{
        "openService",config.openSysMsgService
    }};
}

inline void from_json(const nlohmann::json& j, SysMsgConfig& config)
{
    jsonable::toBoolean(j, "openService", config.openSysMsgService);
}
}
