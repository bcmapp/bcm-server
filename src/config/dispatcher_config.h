#pragma once

#include <utils/jsonable.h>

namespace bcm {

struct DispatcherConfig {
    int concurrency{8};
};

inline void to_json(nlohmann::json& j, const DispatcherConfig& config)
{
    j = nlohmann::json{{"concurrency", config.concurrency}};
}

inline void from_json(const nlohmann::json& j, DispatcherConfig& config)
{
    jsonable::toNumber(j, "concurrency", config.concurrency, jsonable::OPTIONAL);
}

}
