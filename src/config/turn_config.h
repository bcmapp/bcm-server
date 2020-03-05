#pragma once

#include <string>
#include <vector>
#include <utils/jsonable.h>

namespace bcm {

struct TurnConfig {
    std::string secret;
    std::vector<std::string> uris;
};

inline void to_json(nlohmann::json& j, const TurnConfig& config)
{
    j = nlohmann::json{{"secret", config.secret},
                       {"uris", config.uris}};
}

inline void from_json(const nlohmann::json& j, TurnConfig& config)
{
    jsonable::toString(j, "secret", config.secret);
    jsonable::toGeneric(j, "uris", config.uris);
}


}