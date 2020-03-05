#include "push_notification.h"
#include <iostream>
#include <ctime>
#include <config/offline_server_config.h>
#include <utils/log.h>
#include "../crypto/fnv.h"
#include "../dispatcher/dispatch_address.h"

using namespace nlohmann;

namespace bcm {
namespace push {

static const std::string kKeyNotifyType = "notifytype";
static const std::string kKeyContactChat = "contactchat";
static const std::string kKeyGroupChat = "groupchat";
static const std::string kKeySystemChat = "systemmsg";
static const std::string kKeyFriendReq = "friendreq";
static const std::string kKeyUid = "uid";
static const std::string kKeyMid = "mid";
static const std::string kKeyGid = "gid";
static const std::string kFriendReqType = "type";
static const std::string kFriendReqPayload = "payload";
static const std::string kBcmDataTargetKey = "targetid";

static const std::string kNotifyTypeChat = "1";
static const std::string kNotifyTypeGroup = "2";
static const std::string kNotifyTypeSystem = "3";
static const std::string kNotifyTypeFriendReq = "4";

void Notification::chat(const std::string& senderUid)
{
    m_content.clear();
    m_content[kKeyNotifyType] = kNotifyTypeChat;
    m_content[kKeyContactChat][kKeyUid] = senderUid;
}

void Notification::group(const std::string& groupId, const std::string& messageId)
{
    m_content.clear();
    m_content[kKeyNotifyType] = kNotifyTypeGroup;
    m_content[kKeyGroupChat][kKeyMid] = messageId;
    m_content[kKeyGroupChat][kKeyGid] = groupId;
}

void Notification::system(const SysMsgContent& msg)
{
    m_content.clear();
    m_content[kKeyNotifyType] = kNotifyTypeSystem;
    m_content[kKeySystemChat] = msg;
}
void Notification::friendReq(const std::string& senderUid, const std::string& payload, int32_t type)
{
    m_content.clear();
    m_content[kKeyNotifyType] = kNotifyTypeFriendReq;
    m_content[kKeyFriendReq][kKeyUid] = senderUid;
    m_content[kKeyFriendReq][kFriendReqType] = type;
    m_content[kKeyFriendReq][kFriendReqPayload] = payload;
}

void Notification::clearApnsChat(nlohmann::json& j)
{
    std::string  apnsType;
    jsonable::toString(j, kKeyNotifyType, apnsType, jsonable::OPTIONAL);
    
    if (kNotifyTypeChat == apnsType) {
        j.clear();
        j[kKeyNotifyType] = kNotifyTypeChat;
    }
}

void Notification::setTargetAddress(const DispatchAddress& address)
{
    if (!address.getUid().empty()) {
        uint32_t  targetId = FNV::hash(address.getUid().c_str(), address.getUid().size());
        m_content[kBcmDataTargetKey] = targetId;
    }
    m_targetAddress = address;
}

std::string Notification::content() const
{
    return m_content.dump();
}

void Notification::setDeviceInfo(const Device& device)
{
    this->setApnsType(device.apntype());
    this->setApnsId(device.apnid());
    this->setVoipApnsId(device.voipapnid());
    this->setFcmId(device.gcmid());
    this->setUmengId(device.umengid());
    this->setClientVersion(device.clientversion());
}

std::string Notification::getPushType() const
{
    auto useUmengFirst = [](const ClientVersion& cv) {
        if (cv.ostype() != ClientVersion::OSTYPE_ANDROID) {
            return false;
        }

        static std::string modelList[] = {"xiaomi", "huawei"};
        for (auto& model : modelList) {
            std::string cvmodel = cv.phonemodel();
            std::transform(cvmodel.begin(), cvmodel.end(), cvmodel.begin(), ::tolower);
            if (cvmodel.find(model) != std::string::npos) {
                LOGI << "phone: " << cvmodel;
                return true;
            }
        }
        return false;
    };

    if (m_clientVersion.ostype() == ClientVersion::OSTYPE_ANDROID) {
        if (useUmengFirst(m_clientVersion) && !m_umengId.empty()) {
            return TYPE_SYSTEM_PUSH_UMENG;
        }

        if (!m_fcmId.empty()) {
            return TYPE_SYSTEM_PUSH_FCM;
        }

        if (!m_umengId.empty()) {
            return TYPE_SYSTEM_PUSH_UMENG;
        }

        return "";
    } else if (m_clientVersion.ostype() == ClientVersion::OSTYPE_IOS) {
        return TYPE_SYSTEM_PUSH_APNS;
    }
    return "";
}

bool  Notification::isSupportApnsPush() const
{
    if (m_clientVersion.ostype() == ClientVersion::OSTYPE_IOS) {
        std::vector<std::string> versionPair;
        boost::split(versionPair, m_clientVersion.osversion(), boost::is_any_of("."));
        if (versionPair.size() > 1) {
            try {
                int32_t  osv =  std::stoi(versionPair[0]);
                if (osv >= 13) {
                    return true;
                }
            } catch(std::exception const& e) {
                LOGE << "uid: " << m_targetAddress.getUid()
                     << ", osversion: " << m_clientVersion.osversion()
                     << ", what: " << e.what() ;
                return false;
            }
        }
    }
    return false;
}

} // namespace push
} // namespace bcm

