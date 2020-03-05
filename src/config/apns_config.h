#pragma once

#include <boost/core/ignore_unused.hpp>

#include <string>
#include <utils/jsonable.h>

namespace bcm {
// -----------------------------------------------------------------------------
// Section: ApnsEntry
// -----------------------------------------------------------------------------
struct ApnsEntry {
    std::string bundleId;
    bool sandbox{false};
    bool defaultSender{false};
    std::string type;
    std::string certFile;
    std::string keyFile;
};

inline void to_json(nlohmann::json& j, const ApnsEntry& e)
{
    j = nlohmann::json{{"bundleId", e.bundleId},
                       {"sandbox", e.sandbox},
                       {"defaultSender", e.defaultSender},
                       {"type", e.type},
                       {"certFile", e.certFile},
                       {"keyFile", e.keyFile}};
}

inline void from_json(const nlohmann::json& j, ApnsEntry& e)
{
    jsonable::toString(j, "bundleId", e.bundleId);
    jsonable::toBoolean(j, "sandbox", e.sandbox);
    jsonable::toBoolean(j, "defaultSender", e.defaultSender);
    jsonable::toString(j, "type", e.type);
    jsonable::toString(j, "certFile", e.certFile);
    jsonable::toString(j, "keyFile", e.keyFile);
}

// -----------------------------------------------------------------------------
// Section: ApnsConfig
// -----------------------------------------------------------------------------
struct ApnsConfig {
    int32_t expirySecs;
    int32_t resendDelayMilliSecs;
    int32_t maxResendCount;
    std::vector<ApnsEntry> entries;
};

inline void to_json(nlohmann::json& j, const ApnsConfig& c)
{
    j = nlohmann::json{{"expirySecs", c.expirySecs},
                       {"resendDelayMilliSecs", c.resendDelayMilliSecs},
                       {"maxResendCount", c.maxResendCount},
                       {"entries", c.entries}};
}

inline void from_json(const nlohmann::json& j, ApnsConfig& c)
{
    jsonable::toNumber(j, "expirySecs", c.expirySecs);
    jsonable::toNumber(j, "resendDelayMilliSecs", c.resendDelayMilliSecs);
    jsonable::toNumber(j, "maxResendCount", c.maxResendCount);
    jsonable::toGeneric(j, "entries", c.entries);
}

} // namespace bcm