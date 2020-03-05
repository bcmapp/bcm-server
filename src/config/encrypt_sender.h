#pragma once

#include <utils/jsonable.h>

namespace bcm {

struct EncryptSenderConfig {
    uint64_t iosVersion{0};
    uint64_t androidVersion{0};
    bool plainUidSupport{true};
};

inline void to_json(nlohmann::json& j, const EncryptSenderConfig& config)
{
    j = nlohmann::json{{"iosVersion", config.iosVersion},
                       {"androidVersion", config.androidVersion},
                       {"plainUidSupport", config.plainUidSupport}};
}

inline void from_json(const nlohmann::json& j, EncryptSenderConfig& config)
{
    jsonable::toNumber(j, "iosVersion", config.iosVersion);
    jsonable::toNumber(j, "androidVersion", config.androidVersion);
    jsonable::toBoolean(j, "plainUidSupport", config.plainUidSupport);
}


}