#pragma once

#include <string>
#include <utils/jsonable.h>

namespace bcm {

struct FcmConfig {
    std::string apiKey;
};

inline void to_json(nlohmann::json& j, const FcmConfig& config)
{
    j = nlohmann::json{
        {"apiKey", config.apiKey}
    };
}

inline void from_json(const nlohmann::json& j, FcmConfig& config)
{
    jsonable::toGeneric(j, "apiKey", config.apiKey);
}

} // namespace bcm {