#pragma once

#include <utils/jsonable.h>

namespace bcm {

struct SizeCheckConfig {
    size_t contactsSize = 1073741824;
    size_t messageSize = 200 * 1024 * 1024;
    size_t onetimeKeySize = 100;
};

inline void to_json(nlohmann::json& j, const SizeCheckConfig& config)
{
    j = nlohmann::json{{
                               "contactsSize", config.contactsSize
                       },
                       {
                               "messageSize",  config.messageSize
                       },
                       {
                               "onetimeKeySize", config.onetimeKeySize
                       }};
}

inline void from_json(const nlohmann::json& j, SizeCheckConfig& config)
{
    jsonable::toNumber(j, "contactsSize", config.contactsSize);
    jsonable::toNumber(j, "messageSize", config.messageSize);
    jsonable::toNumber(j, "onetimeKeySize", config.onetimeKeySize);

}
}
