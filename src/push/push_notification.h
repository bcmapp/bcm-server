#pragma once

#include <nlohmann/json.hpp>
#include <controllers/system_entities.h>
#include <dispatcher/dispatch_address.h>
#include <proto/dao/device.pb.h>

namespace bcm {
namespace push {

enum Classes {
    SILENT = -1, // 不推送，对应原来的silent = true
    NORMAL = 0, // 正常推送，对应原来的silent = false
    CALLING = 1, // 电话推送，发送紧急通知和特定文案
};

class Notification {
public:
    Notification() = default;

    Notification(const Notification& other) = default;
    Notification(Notification&& other) = default;
    Notification& operator=(const Notification& other) = default;
    Notification& operator=(Notification&& other) = default;

    void chat(const std::string& senderUid);
    void group(const std::string& groupId, const std::string& messageId);
    void system(const SysMsgContent& msg);
    void friendReq(const std::string& senderUid, const std::string& payload, int32_t type = 0);
    static void clearApnsChat(nlohmann::json& j);

    const nlohmann::json& rawContent() const { return m_content; }
    std::string content() const;

    std::string getPushType() const;

    bool  isSupportApnsPush() const;
    
    // no real content
    void setBadge(int32_t n) { m_badge = n; }
    int32_t badge() const { return m_badge; }

    void setTargetAddress(const DispatchAddress& address);
    const DispatchAddress& targetAddress() const { return m_targetAddress; }

    void setApnsType(std::string type) { m_apns.type = std::move(type); }
    const std::string& apnsType() const { return m_apns.type; }

    void setApnsId(std::string id) { m_apns.id = std::move(id); }
    const std::string& apnsId() const { return m_apns.id; }

    void setVoipApnsId(std::string id) { m_apns.voipId = std::move(id); }
    const std::string& voipApnsId() const { return m_apns.voipId; }

    void setFcmId(std::string id) { m_fcmId = std::move(id); }
    const std::string& fcmId() const { return m_fcmId; }

    void setUmengId(std::string id) { m_umengId = std::move(id); }
    const std::string& umengId() const { return m_umengId; }

    void setClientVersion(ClientVersion version) { m_clientVersion = std::move(version); }
    const ClientVersion& clientVersion() { return m_clientVersion; }

    void setDeviceInfo(const Device& device);

    void setClass(Classes pushClass) { m_pushClass = pushClass; }
    Classes getClass() const { return m_pushClass; }

private:
    nlohmann::json m_content;
    int32_t m_badge{-1};

    // device info
    DispatchAddress m_targetAddress{"", 0};
    struct {
        std::string type;
        std::string id;
        std::string voipId;
    } m_apns;
    std::string m_fcmId;
    std::string m_umengId;
    ClientVersion m_clientVersion;
    Classes m_pushClass {Classes::NORMAL};


    friend void to_json(nlohmann::json& j, const Notification& noti);
    friend void from_json(const nlohmann::json& j, Notification& noti);
};

inline void to_json(nlohmann::json& j, const Notification& noti) {
    j = nlohmann::json{{"content", noti.m_content},
                       {"badge", noti.m_badge},
                       {"target", noti.m_targetAddress.getSerialized()},
                       {"apnsType", noti.m_apns.type},
                       {"apnsId", noti.m_apns.id},
                       {"apnsVoipId", noti.m_apns.voipId},
                       {"fcmId", noti.m_fcmId},
                       {"umengId", noti.m_umengId},
                       {"clientOsType", static_cast<int>(noti.m_clientVersion.ostype())},
                       {"clientOsVersion", noti.m_clientVersion.osversion()},
                       {"clientPhoneModel", noti.m_clientVersion.phonemodel()},
                       {"clientBcmVersion", noti.m_clientVersion.bcmversion()},
                       {"clientBcmBuildCode", noti.m_clientVersion.bcmbuildcode()},
                       {"pushClass", static_cast<int>(noti.m_pushClass)}
                       };
}

inline void from_json(const nlohmann::json& j, Notification& noti) {
    jsonable::toGeneric(j, "content", noti.m_content);
    jsonable::toNumber(j, "badge", noti.m_badge);

    std::string serializedTarget;
    jsonable::toString(j, "target", serializedTarget);
    auto target = DispatchAddress::deserialize(serializedTarget);
    if (target == boost::none) {
        throw nlohmann::detail::type_error::create(302, "parse dispatch address failed");
    }
    noti.m_targetAddress = *target;

    jsonable::toString(j, "apnsType", noti.m_apns.type);
    jsonable::toString(j, "apnsId", noti.m_apns.id);
    jsonable::toString(j, "apnsVoipId", noti.m_apns.voipId);
    jsonable::toString(j, "fcmId", noti.m_fcmId);
    jsonable::toString(j, "umengId", noti.m_umengId);

    int osType;
    jsonable::toNumber(j, "clientOsType", osType);
    noti.m_clientVersion.set_ostype(static_cast<ClientVersion::OSType>(osType));

    std::string tmp;
    jsonable::toString(j, "clientOsVersion", tmp);
    noti.m_clientVersion.set_osversion(tmp);
    jsonable::toString(j, "clientPhoneModel", tmp);
    noti.m_clientVersion.set_phonemodel(tmp);
    jsonable::toString(j, "clientBcmVersion", tmp);
    noti.m_clientVersion.set_bcmversion(tmp);

    uint64_t buildCode;
    jsonable::toNumber(j, "clientBcmBuildCode", buildCode);
    noti.m_clientVersion.set_bcmbuildcode(buildCode);

    int pushClass;
    jsonable::toNumber(j, "pushClass", pushClass);
    noti.m_pushClass = static_cast<push::Classes>(pushClass);
}

struct Notifications {
    std::vector<push::Notification> content;
};

inline void to_json(nlohmann::json& j, const Notifications& notis)
{
    j = nlohmann::json{{"notifications", notis.content}};
}

inline void from_json(const nlohmann::json& j, Notifications& notis)
{
    jsonable::toGeneric(j, "notifications", notis.content);
}


} // namespace push
} // namespace bcm
