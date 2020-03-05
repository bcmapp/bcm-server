#pragma once

#include <string>
#include <utils/jsonable.h>

namespace bcm {

struct UmengConfig {
    std::string appMasterSecret;
    std::string appKey;
    std::string appMasterSecretV2;
    std::string appKeyV2;
};

inline void to_json(nlohmann::json& j, const UmengConfig& config)
{
    j = nlohmann::json{{"appMasterSecret", config.appMasterSecret},
                       {"apiKey", config.appKey},
                       {"appMasterSecretV2", config.appMasterSecretV2},
                       {"apiKeyV2", config.appKeyV2}};
}

inline void from_json(const nlohmann::json& j, UmengConfig& config)
{
    jsonable::toString(j, "appMasterSecret", config.appMasterSecret);
    jsonable::toString(j, "apiKey", config.appKey);
    jsonable::toString(j, "appMasterSecretV2", config.appMasterSecretV2);
    jsonable::toString(j, "apiKeyV2", config.appKeyV2);
}

} // namespace bcm {