#pragma once

#include <string>
#include <vector>
#include <utils/jsonable.h>

namespace bcm {

struct RedisConfig {
    std::string ip;
    int port;
    std::string password;
	std::string regkey;
};

inline void to_json(nlohmann::json& j, const RedisConfig& config)
{
    j = nlohmann::json{
        {"ip", config.ip},
        {"port", config.port},
        {"password", config.password},
		{"regkey", config.regkey},
    };
}

inline void from_json(const nlohmann::json& j, RedisConfig& config)
{
    jsonable::toString(j, "ip", config.ip);
    jsonable::toNumber(j, "port", config.port);
    jsonable::toString(j, "password", config.password);
	jsonable::toString(j, "regkey", config.regkey);
}

}