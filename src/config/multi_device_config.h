#pragma once

#include <utils/jsonable.h>

namespace bcm {

struct MultiDeviceConfig {
    uint64_t qrcodeExpireSecs = 180;
    bool needUpgrade = false;
};

inline void to_json(nlohmann::json& j, const MultiDeviceConfig& config)
{
    j = nlohmann::json{{"qrcodeExpireSecs", config.qrcodeExpireSecs,
                        "needUpgrade", config.needUpgrade}};
}

inline void from_json(const nlohmann::json& j, MultiDeviceConfig& config)
{
    jsonable::toNumber(j, "qrcodeExpireSecs", config.qrcodeExpireSecs, jsonable::OPTIONAL);
    jsonable::toBoolean(j, "needUpgrade", config.needUpgrade, jsonable::OPTIONAL);
}
} // namespace bcm
