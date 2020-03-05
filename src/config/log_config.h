#pragma once

#include <string>
#include <utils/jsonable.h>

namespace bcm {

struct LogConfig {
    int level{0};
    std::string directory;
    bool console{false};
    bool autoflush{false};
};

inline void to_json(nlohmann::json& j, const LogConfig& config)
{
    j = nlohmann::json{{"level", config.level},
                       {"directory", config.directory},
                       {"console", config.console},
                       {"autoflush", config.autoflush}};
}

inline void from_json(const nlohmann::json& j, LogConfig& config)
{
    jsonable::toNumber(j, "level", config.level);
    jsonable::toString(j, "directory", config.directory);
    jsonable::toBoolean(j, "console", config.console);
    jsonable::toBoolean(j, "autoflush", config.autoflush);
}


}
