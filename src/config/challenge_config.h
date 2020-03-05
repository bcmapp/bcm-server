#pragma once

#include <utils/jsonable.h>

namespace bcm {

struct ChallengeConfig {
    uint32_t difficulty{15};
    uint64_t expiration{10 * 60 * 1000}; //in millis, default: 10 minutes
};

inline void to_json(nlohmann::json& j, const ChallengeConfig& config)
{
    j = nlohmann::json{{"difficulty", config.difficulty},
                       {"expiration", config.expiration}};
}

inline void from_json(const nlohmann::json& j, ChallengeConfig& config)
{
    jsonable::toNumber(j, "difficulty", config.difficulty, jsonable::OPTIONAL);
    jsonable::toNumber(j, "expiration", config.expiration, jsonable::OPTIONAL);
}

}
