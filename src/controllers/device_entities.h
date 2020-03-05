#pragma once

#include <utils/jsonable.h>
#include "proto/device/multi_device.pb.h"

namespace bcm {

struct DeviceStatus {
    uint32_t id;
    std::string name;
    std::string model;
    int32_t status;
    int64_t lastSeen;
};

inline void to_json(nlohmann::json& j, const DeviceStatus& deviceStatus)
{
    j = nlohmann::json{{"id", deviceStatus.id},
                       {"name", deviceStatus.name},
                       {"model", deviceStatus.model},
                       {"status", deviceStatus.status},
                       {"lastSeen", deviceStatus.lastSeen}};
}

inline void from_json(const nlohmann::json& j, DeviceStatus& deviceStatus)
{
    jsonable::toNumber(j, "id", deviceStatus.id);
    jsonable::toString(j, "name", deviceStatus.name);
    jsonable::toString(j, "model", deviceStatus.model);
    jsonable::toNumber(j, "status", deviceStatus.status);
    jsonable::toNumber(j, "lastSeen", deviceStatus.lastSeen);
}

struct DeviceStatusRes {
    std::vector<DeviceStatus> devices;
};

inline void to_json(nlohmann::json& j, const DeviceStatusRes& devices)
{
    j = nlohmann::json{{"devices", devices.devices}};
}

inline void from_json(const nlohmann::json& j, DeviceStatusRes& res)
{
    jsonable::toGeneric(j, "devices", res.devices);
}

struct DeviceManageAction {
    int32_t action;
    uint32_t deviceId;
    int64_t timestamp;
    int64_t nounce;
    std::string signature;
};

inline void to_json(nlohmann::json& j, const DeviceManageAction& action)
{
    j = nlohmann::json{{"action", action.action},
                       {"deviceId", action.deviceId},
                       {"timestamp", action.timestamp},
                       {"nounce", action.nounce},
                       {"signature", action.signature}};
}

inline void from_json(const nlohmann::json& j, DeviceManageAction& action)
{
    jsonable::toNumber(j, "action", action.action);
    jsonable::toNumber(j, "deviceId", action.deviceId);
    jsonable::toNumber(j, "timestamp", action.timestamp);
    jsonable::toNumber(j, "nounce", action.nounce);
    jsonable::toString(j, "signature", action.signature);
}

inline void to_json(nlohmann::json& j, const DeviceLoginReqInfo& prepare)
{
    j = nlohmann::json{{"nounce", prepare.nounce()},
                       {"signature", prepare.signature()},
                       {"publicKey", prepare.publickey()},
                       {"deviceId", prepare.deviceid()},
                       {"deviceName", prepare.devicename()}};
}
inline void from_json(const nlohmann::json& j, DeviceLoginReqInfo& prepare)
{
    int64_t nounce;
    std::string signature;
    std::string publicKey;
    std::string deviceName;
    uint32_t deviceId;

    jsonable::toNumber(j, "nounce", nounce);
    jsonable::toNumber(j, "deviceId", deviceId, jsonable::OPTIONAL);
    jsonable::toString(j, "signature", signature);
    jsonable::toString(j, "publicKey", publicKey);
    jsonable::toString(j, "deviceName", deviceName);

    prepare.set_nounce(nounce);
    prepare.set_publickey(std::move(publicKey));
    prepare.set_signature(std::move(signature));
    prepare.set_devicename(std::move(deviceName));
    prepare.set_deviceid(deviceId);
}

struct DeviceReqInfo {
    int64_t nounce;
    std::string signature;
    std::string publicKey;
    std::string deviceName;
    uint32_t deviceId;
};

struct DeviceLoginRes {
    std::string requestId;
    int64_t expireTime;
};

inline void to_json(nlohmann::json& j, const DeviceLoginRes& res)
{
    j = nlohmann::json{{"requestId", res.requestId},
                       {"expireTime", res.expireTime}};
}

inline void from_json(const nlohmann::json& j, DeviceLoginRes& res)
{
    jsonable::toNumber(j, "expireTime", res.expireTime);
    jsonable::toString(j, "requestId", res.requestId);
}

inline void to_json(nlohmann::json& j, const AvatarSyncInfo& avatar)
{
    j = nlohmann::json{{"contentType", avatar.contenttype()},
                       {"contentLength", avatar.contentlength()},
                       {"content", avatar.content()},
                       {"accountPublicKey", avatar.accountpublickey()},
                       {"type", avatar.type()},
                       {"avatarUrl", avatar.avatarurl()},
                       {"nickName", avatar.nickname()},
                       {"digest", avatar.digest()}};
}

inline void from_json(const nlohmann::json& j, AvatarSyncInfo& avatar)
{
    std::string contentType;
    uint64_t contentLength;
    std::string content;
    std::string digest;
    std::string accountPublicKey;
    uint32_t type;
    std::string avatarUrl;
    std::string nickName;


    jsonable::toNumber(j, "type", type);
    jsonable::toString(j, "accountPublicKey", accountPublicKey, jsonable::OPTIONAL);

    avatar.set_type(type);

    if (1 == type) {
        jsonable::toString(j, "avatarUrl", avatarUrl, jsonable::NOT_EMPTY);
        jsonable::toString(j, "nickName", nickName, jsonable::NOT_EMPTY);

        avatar.set_avatarurl(std::move(avatarUrl));
        avatar.set_nickname(std::move(nickName));
    } else {
        jsonable::toString(j, "contentType", contentType, jsonable::NOT_EMPTY);
        jsonable::toNumber(j, "contentLength", contentLength);
        jsonable::toString(j, "content", content, jsonable::NOT_EMPTY);
        jsonable::toString(j, "digest", digest, jsonable::NOT_EMPTY);

        avatar.set_contenttype(std::move(contentType));
        avatar.set_contentlength(contentLength);
        avatar.set_content(std::move(content));
        avatar.set_digest(std::move(digest));
    }

    if (!accountPublicKey.empty()) {
        avatar.set_accountpublickey(std::move(accountPublicKey));
    }
}

inline void to_json(nlohmann::json& j, const DeviceAuthInfo& auth)
{
    j = nlohmann::json{{"uid", auth.uid()},
                       {"accountPublicKey", auth.accountpublickey()},
                       {"deviceId", auth.deviceid()},
                       {"requestId", auth.requestid()},
                       {"accountSignature", auth.accountsignature()}};
}

inline void from_json(const nlohmann::json& j, DeviceAuthInfo& auth)
{
    std::string uid;
    std::string accountPublicKey;
    std::uint32_t deviceId;
    std::string requestId;
    std::string accountSignature;

    jsonable::toString(j, "uid", uid, jsonable::OPTIONAL);
    jsonable::toNumber(j, "deviceId", deviceId);
    jsonable::toString(j, "accountPublicKey", accountPublicKey, jsonable::OPTIONAL);
    jsonable::toString(j, "requestId", requestId);
    jsonable::toString(j, "accountSignature", accountSignature);

    auth.set_uid(std::move(uid));
    auth.set_requestid(std::move(requestId));
    auth.set_accountpublickey(std::move(accountPublicKey));
    auth.set_accountsignature(std::move(accountSignature));
    auth.set_deviceid(deviceId);

}

}
