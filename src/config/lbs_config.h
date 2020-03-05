#pragma once

#include <utils/jsonable.h>

namespace bcm {

struct LbsConfig {
    std::string host;
    int port;
    std::string target;
    std::string name;
};

inline void to_json(nlohmann::json& j, const LbsConfig& config)
{
    j = nlohmann::json{{"host", config.host},
                       {"port", config.port},
                       {"target", config.target},
                       {"name", config.name}};
}

inline void from_json(const nlohmann::json& j, LbsConfig& config)
{
    jsonable::toString(j, "host", config.host);
    jsonable::toNumber(j, "port", config.port);
    jsonable::toString(j, "target", config.target);
    jsonable::toString(j, "name", config.name);
}

}
