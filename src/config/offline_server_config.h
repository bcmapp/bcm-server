#pragma once

#include <boost/core/ignore_unused.hpp>

#include <string>
#include <vector>
#include <utils/jsonable.h>

namespace bcm {
// -----------------------------------------------------------------------------
// Section: ApnsConfig
// -----------------------------------------------------------------------------

const std::string  TYPE_SYSTEM_PUSH_APNS  = "apns";
const std::string  TYPE_SYSTEM_PUSH_UMENG = "umeng";
const std::string  TYPE_SYSTEM_PUSH_FCM   = "fcm";

struct OfflineServerConfig {
    int32_t pushRoundInterval;
    int32_t pushThreadNumb;
    int32_t eventThreadNumb;
    std::string redisPartition;
    bool    isPush;
    std::vector<std::string> pushType;

    bool checkPushType(const std::string& checkingType)
    {
        for (const auto& it : pushType) {
            if (it == checkingType) {
                return true;
            }
        }
        return false;
    }
};

inline void to_json(nlohmann::json& j, const OfflineServerConfig& c)
{
    j = nlohmann::json{{"pushRoundInterval", c.pushRoundInterval},
                       {"pushThreadNumb", c.pushThreadNumb},
                       {"eventThreadNumb", c.eventThreadNumb},
                       {"redisPartition", c.redisPartition},
                       {"isPush", c.isPush},
                       {"pushType", c.pushType}};
}

inline void from_json(const nlohmann::json& j, OfflineServerConfig& c)
{
    jsonable::toNumber(j, "pushRoundInterval", c.pushRoundInterval);
    jsonable::toNumber(j, "pushThreadNumb", c.pushThreadNumb);
    jsonable::toNumber(j, "eventThreadNumb", c.eventThreadNumb);
    jsonable::toString(j, "redisPartition", c.redisPartition);
    jsonable::toBoolean(j, "isPush", c.isPush);
    jsonable::toGeneric(j, "pushType", c.pushType);
}

} // namespace bcm