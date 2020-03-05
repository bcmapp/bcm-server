#pragma once

#include <list>
#include <utils/jsonable.h>
#include <push/push_notification.h>

namespace bcm {

struct IncomingMessage {
    int type;
    std::string destination;
    uint32_t destinationDeviceId;
    int destinationRegistrationId;
    std::string body; // Deprecated
    std::string content;
    std::string relay;
    uint64_t timestamp;
    bool silent{false}; // Deprecated
    int push;
};

static inline void to_json(nlohmann::json& j, const IncomingMessage& entity)
{
    j = nlohmann::json{{"type",                      entity.type},
                       {"destination",               entity.destination},
                       {"destinationDeviceId",       entity.destinationDeviceId},
                       {"destinationRegistrationId", entity.destinationRegistrationId},
                       {"body",                      entity.body},
                       {"content",                   entity.content},
                       {"relay",                     entity.relay},
                       {"timestamp",                 entity.timestamp},
                       {"silent",                    entity.silent},
                       {"push",                      entity.push}
    };
}

static inline void from_json(const nlohmann::json& j, IncomingMessage& entity)
{
    jsonable::toNumber(j, "type", entity.type);
    jsonable::toString(j, "destination", entity.destination, jsonable::OPTIONAL);
    jsonable::toNumber(j, "destinationDeviceId", entity.destinationDeviceId);
    jsonable::toNumber(j, "destinationRegistrationId", entity.destinationRegistrationId);
    jsonable::toString(j, "body", entity.body, jsonable::OPTIONAL);
    jsonable::toString(j, "content", entity.content);
    jsonable::toString(j, "relay", entity.relay, jsonable::OPTIONAL);
    jsonable::toNumber(j, "timestamp", entity.timestamp, jsonable::OPTIONAL);
    jsonable::toBoolean(j, "silent", entity.silent, jsonable::OPTIONAL);

    if (j.find("push") != j.end()) {
        jsonable::toNumber(j, "push", entity.push);
    } else {
        entity.push = entity.silent ? push::Classes::SILENT : push::Classes::NORMAL;
    }
}

struct IncomingMessageList {
    std::list<IncomingMessage> messages;
    std::string relay;
    uint64_t timestamp;
};

static inline void to_json(nlohmann::json& j, const IncomingMessageList& entity)
{
    j = nlohmann::json{{"messages",  entity.messages},
                       {"relay",     entity.relay},
                       {"timestamp", entity.timestamp},
    };
}

static inline void from_json(const nlohmann::json& j, IncomingMessageList& entity)
{
    jsonable::toGeneric(j, "messages", entity.messages);
    jsonable::toString(j, "relay", entity.relay, jsonable::OPTIONAL);
    jsonable::toNumber(j, "timestamp", entity.timestamp);
}

struct SendMessageResponse {
    bool needsSync;

    SendMessageResponse()
        : needsSync(false)
    {
    }

    explicit SendMessageResponse(bool isSync)
        : needsSync(isSync)
    {
    }

    ~SendMessageResponse() = default;

};

static inline void to_json(nlohmann::json& j, const SendMessageResponse& entity)
{
    j = nlohmann::json{{"needsSync", entity.needsSync}};
}

static inline void from_json(const nlohmann::json& j, SendMessageResponse& entity)
{
    jsonable::toBoolean(j, "needsSync", entity.needsSync);
}

struct MismatchedDevices {
    std::vector<uint32_t> missingDevices;
    std::vector<uint32_t> extraDevices;
};

inline void to_json(nlohmann::json& j, const MismatchedDevices& devices) {
    j = nlohmann::json{{"missingDevices", devices.missingDevices},
                       {"extraDevices", devices.extraDevices}};
}

inline void from_json(const nlohmann::json& j, MismatchedDevices& devices) {
    jsonable::toGeneric(j, "missingDevices", devices.missingDevices);
    jsonable::toGeneric(j, "extraDevices", devices.extraDevices);
}

struct StaleDevices {
    std::vector<uint32_t> staleDevices;
};

inline void to_json(nlohmann::json& j, const StaleDevices& devices) {
    j = nlohmann::json{{"staleDevices", devices.staleDevices}};
}

inline void from_json(const nlohmann::json& j, StaleDevices& devices) {
    jsonable::toGeneric(j, "staleDevices", devices.staleDevices);
}

}
