#pragma once

#include <string>
#include <vector>
#include <set>

#include <boost/core/ignore_unused.hpp>

#include "utils/jsonable.h"
#include "group_common_entities.h"
#include "proto/dao/group.pb.h"
#include "dao/group_users.h"
#include "group/message_type.h"
#include "proto/dao/pending_group_user.pb.h"
#include "proto/dao/account.pb.h"
#include "keys_entities.h"

namespace bcm {
constexpr uint32_t kMaxGroupNameLength = 512;
constexpr uint32_t kMaxGroupIconLength = 512;
constexpr uint32_t kMaxGroupIntroLength = 2048;
constexpr uint32_t kMaxGroupNoticeLength = 4096;
constexpr uint32_t kMaxGroupKeyLength = 1024;
constexpr uint32_t kMaxGroupProofLength = 512;
constexpr uint32_t kMaxGroupCreateMemberCount = 128;
constexpr uint32_t kMaxGroupUidLength = 65;
constexpr uint32_t kMaxGroupPlainChannelKeyLength  = 256;
constexpr uint32_t kMaxGroupChannelLength = 1024;
constexpr uint32_t kMaxGroupBatchCommonV3 = 10;
constexpr uint32_t kMaxGroupBatchFetchLatestKeysV3 = 5;
constexpr int kMaxMemberQueryCount = 500;
constexpr size_t kMaxGroupInfoSecretLength = 1024;
constexpr size_t kMaxQrCodeSettingLength = 1024;
constexpr size_t kMaxShareSignatureLength = 512;
constexpr size_t kMaxShareAndOwnerConfirmSignatureLength = 512;
constexpr size_t kMaxPendingGroupUserSignatureLength = 2048;
constexpr size_t kMaxAccountUidLength = 64;
constexpr size_t kMaxPendingGroupUserCommentLength = 2048;


struct SimpleGroupMemberInfo {
    std::string uid;
    std::string nick;
    GroupUser::Role role;

    SimpleGroupMemberInfo(const std::string& id, const std::string& nickName, GroupUser::Role userRole)
        :uid(id), nick(nickName), role(userRole){}
};

inline void to_json(nlohmann::json& j, const SimpleGroupMemberInfo& info)
{
    j = nlohmann::json{{"uid", info.uid},
                       {"nick", info.nick},
                       {"role", static_cast<int32_t>(info.role)}};
}

struct CreateGroupBodyV2 {
    std::string name; // TODO: deprecated
    std::string icon; // TODO: deprecated
    std::string intro;
    int32_t broadcast{0};
    std::string ownerKey;
    std::string ownerSecretKey; //added for group share
    std::vector<std::string> members;
    std::vector<std::string> membersGroupInfoSecrets; //added for group share
    std::vector<std::string> memberKeys;
    std::string ownerNickname;
    std::string ownerProfileKeys;
    std::string qrCodeSetting;
    int32_t ownerConfirm;
    std::string shareSignature;
    std::string shareAndOwnerConfirmSignature;

    bool check(GroupResponse& response);
};

inline void to_json(nlohmann::json &, const CreateGroupBodyV2 &) {
}

inline void from_json(const nlohmann::json &j, CreateGroupBodyV2 &body) {
    jsonable::toString(j, "name", body.name, jsonable::OPTIONAL);
    jsonable::toString(j, "icon", body.icon, jsonable::OPTIONAL);
    jsonable::toString(j, "intro", body.intro);
    jsonable::toNumber(j, "broadcast", body.broadcast);
    jsonable::toString(j, "owner_key", body.ownerKey, jsonable::OPTIONAL);
    jsonable::toString(j, "owner_group_info_secret", body.ownerSecretKey, jsonable::OPTIONAL);
    jsonable::toGeneric(j, "members", body.members);
    jsonable::toGeneric(j, "member_group_info_secrets", body.membersGroupInfoSecrets, jsonable::OPTIONAL);
    jsonable::toGeneric(j, "member_keys", body.memberKeys, jsonable::OPTIONAL);
    jsonable::toString(j, "owner_nickname", body.ownerNickname, jsonable::OPTIONAL);
    jsonable::toString(j, "owner_profile_keys", body.ownerProfileKeys, jsonable::OPTIONAL);
    jsonable::toString(j, "share_qr_code_setting", body.qrCodeSetting);
    jsonable::toNumber(j, "owner_confirm", body.ownerConfirm);
    jsonable::toString(j, "share_sig", body.shareSignature);
    jsonable::toString(j, "share_and_owner_confirm_sig", body.shareAndOwnerConfirmSignature);
}

struct GroupKeyEntryV0 {
    std::string uid {""};
    uint32_t deviceId {0};
    std::string key {""};
};

inline void to_json(nlohmann::json& j, const GroupKeyEntryV0& e) {
    j = nlohmann::json{{"uid", e.uid},
                       {"device_id", e.deviceId},
                       {"key", e.key}
    };
}

inline void from_json(const nlohmann::json& j, GroupKeyEntryV0& e) {
    jsonable::toString(j, "uid", e.uid);
    jsonable::toNumber(j, "device_id", e.deviceId);
    jsonable::toString(j, "key", e.key);
}

struct GroupKeyEntryV1 {
    std::string key {""};
};

inline void to_json(nlohmann::json& j, const GroupKeyEntryV1& e) {
    j = nlohmann::json{{"key", e.key}
    };
}

inline void from_json(const nlohmann::json& j, GroupKeyEntryV1& e) {
    jsonable::toString(j, "key", e.key);
}

struct GroupKeysJson {
    int32_t encryptVersion {0};
    std::vector<GroupKeyEntryV0> keysV0;
    GroupKeyEntryV1 keysV1;
};

inline void to_json(nlohmann::json& j, const GroupKeysJson& k) {
    j = nlohmann::json{{"encrypt_version", k.encryptVersion},
                       {"keys_v0", k.keysV0},
                       {"keys_v1", k.keysV1}
    };
}

inline void from_json(const nlohmann::json& j, GroupKeysJson& k) {
    jsonable::toNumber(j, "encrypt_version", k.encryptVersion);
    jsonable::toGeneric(j, "keys_v0", k.keysV0);
    jsonable::toGeneric(j, "keys_v1", k.keysV1);
}

struct CreateGroupBodyV3 {
    std::string name; // TODO: deprecated
    std::string icon; // TODO: deprecated
    std::string intro;
    int32_t broadcast{0};
    std::string ownerSecretKey; //added for group share
    std::vector<std::string> members;
    std::vector<std::string> membersGroupInfoSecrets; //added for group share
    std::string ownerNickname;
    std::string ownerProfileKeys;
    std::string qrCodeSetting;
    int32_t ownerConfirm;
    std::string shareSignature;
    std::string shareAndOwnerConfirmSignature;

    // V3 ADD
    std::string encryptedGroupInfoSecret; // use for qr_code
    std::vector<std::string> memberProofs; // for proof member is actually in group
    std::string ownerProof; // owner proof
    GroupKeysJson groupKeys; // X3DH
    int32_t groupKeysMode; // 0: most encryption group, 1: large group
    std::string encryptedEphemeralKey;

    bool check(GroupResponse& response);
};

inline void to_json(nlohmann::json &, const CreateGroupBodyV3 &) {
}

inline void from_json(const nlohmann::json &j, CreateGroupBodyV3 &body) {
    jsonable::toString(j, "name", body.name, jsonable::OPTIONAL);
    jsonable::toString(j, "icon", body.icon, jsonable::OPTIONAL);
    jsonable::toString(j, "intro", body.intro);
    jsonable::toNumber(j, "broadcast", body.broadcast);
    jsonable::toString(j, "owner_group_info_secret", body.ownerSecretKey, jsonable::OPTIONAL);
    jsonable::toGeneric(j, "members", body.members);
    jsonable::toGeneric(j, "member_group_info_secrets", body.membersGroupInfoSecrets, jsonable::OPTIONAL);
    jsonable::toString(j, "owner_nickname", body.ownerNickname, jsonable::OPTIONAL);
    jsonable::toString(j, "owner_profile_keys", body.ownerProfileKeys, jsonable::OPTIONAL);
    jsonable::toString(j, "share_qr_code_setting", body.qrCodeSetting);
    jsonable::toNumber(j, "owner_confirm", body.ownerConfirm);
    jsonable::toString(j, "share_sig", body.shareSignature);
    jsonable::toString(j, "share_and_owner_confirm_sig", body.shareAndOwnerConfirmSignature);

    // V3 ADD
    jsonable::toString(j, "encrypted_group_info_secret", body.encryptedGroupInfoSecret, jsonable::OPTIONAL);
    jsonable::toGeneric(j, "group_keys", body.groupKeys);
    jsonable::toGeneric(j, "member_proofs", body.memberProofs);
    jsonable::toString(j, "owner_proof", body.ownerProof);
    jsonable::toNumber(j, "group_keys_mode", body.groupKeysMode);
    jsonable::toString(j, "encrypted_ephemeral_key", body.encryptedEphemeralKey);
}

struct CreateGroupBodyV3Resp {
    uint64_t gid;
};

inline void to_json(nlohmann::json& j, const CreateGroupBodyV3Resp& resp) {
    j["gid"] = resp.gid;
}

inline void from_json(const nlohmann::json& j, CreateGroupBodyV3Resp& resp) {
    jsonable::toNumber(j, "gid", resp.gid);
}

struct FetchLatestGroupKeysRequest {
    std::set<uint64_t> gids;
};

inline void to_json(nlohmann::json& j, const FetchLatestGroupKeysRequest& req) {
    j = nlohmann::json{{"gids", req.gids}
    };
}

inline void from_json(const nlohmann::json& j, FetchLatestGroupKeysRequest& req) {
    jsonable::toGeneric(j, "gids", req.gids);
}

struct FetchGroupKeysRequest {
    uint64_t gid;
    std::set<int64_t> versions;
};

inline void to_json(nlohmann::json& j, const FetchGroupKeysRequest& req) {
    j = nlohmann::json{{"gid", req.gid},
                       {"versions", req.versions}
    };
}

inline void from_json(const nlohmann::json& j, FetchGroupKeysRequest& req) {
    jsonable::toNumber(j, "gid", req.gid);
    jsonable::toGeneric(j, "versions", req.versions);
}

struct FetchGroupKeysResponseKey {
    int64_t version;
    int32_t encryptVersion;
    int32_t groupKeysMode;
    GroupKeyEntryV0 keysV0;
    GroupKeyEntryV1 keysV1;
};

inline void to_json(nlohmann::json& j, const FetchGroupKeysResponseKey& keys) {
    j = nlohmann::json{{"version", keys.version},
                       {"encrypt_version", keys.encryptVersion},
                       {"group_keys_mode", keys.groupKeysMode},
                       {"keys_v0", keys.keysV0},
                       {"keys_v1", keys.keysV1}
    };
}

inline void from_json(const nlohmann::json &, FetchGroupKeysResponseKey &) {

}

struct FetchLatestGroupKeysResponseKey {
    uint64_t gid;
    int64_t version;
    int32_t encryptVersion;
    int32_t groupKeysMode;
    GroupKeyEntryV0 keysV0;
    GroupKeyEntryV1 keysV1;
};

inline void to_json(nlohmann::json& j, const FetchLatestGroupKeysResponseKey& keys) {
    j = nlohmann::json{{"gid", keys.gid},
                       {"version", keys.version},
                       {"encrypt_version", keys.encryptVersion},
                       {"group_keys_mode", keys.groupKeysMode},
                       {"keys_v0", keys.keysV0},
                       {"keys_v1", keys.keysV1}
    };
}

inline void from_json(const nlohmann::json &, FetchLatestGroupKeysResponseKey &) {

}

struct FetchLatestGroupKeysResponse {
    std::vector<FetchLatestGroupKeysResponseKey> keys;
};

inline void to_json(nlohmann::json& j, const FetchLatestGroupKeysResponse& keys) {
    j = nlohmann::json{{"keys", keys.keys}
    };
}

inline void from_json(const nlohmann::json &, FetchLatestGroupKeysResponse &) {

}

struct FetchGroupKeysResponse {
    uint64_t gid;
    std::vector<FetchGroupKeysResponseKey> keys;
};

inline void to_json(nlohmann::json& j, const FetchGroupKeysResponse& keys) {
    j = nlohmann::json{{"gid", keys.gid},
                       {"keys", keys.keys}
    };
}

inline void from_json(const nlohmann::json &, FetchGroupKeysResponse &) {

}

struct FireGroupKeysUpdateRequest {
    std::set<uint64_t> gids;
};

inline void to_json(nlohmann::json& j, const FireGroupKeysUpdateRequest& r) {
    j = nlohmann::json{{"gids", r.gids}
    };
}

inline void from_json(const nlohmann::json& j, FireGroupKeysUpdateRequest& r) {
    jsonable::toGeneric(j, "gids", r.gids);
}

struct FireGroupKeysUpdateResponse {
    std::vector<uint64_t> success;
    std::vector<uint64_t> fail;
};

inline void to_json(nlohmann::json& j, const FireGroupKeysUpdateResponse& g) {
    j = nlohmann::json{{"success", g.success},
                       {"fail", g.fail}
    };
}

inline void from_json(const nlohmann::json &, FireGroupKeysUpdateResponse &) {

}

struct UploadGroupKeysRequest {
    uint64_t gid;
    int64_t version;
    int32_t groupKeysMode;
    GroupKeysJson groupKeys;
};

inline void to_json(nlohmann::json &, const UploadGroupKeysRequest &) {
}

inline void from_json(const nlohmann::json& j, UploadGroupKeysRequest &req) {
    jsonable::toNumber(j, "gid", req.gid);
    jsonable::toNumber(j, "version", req.version);
    jsonable::toNumber(j, "group_keys_mode", req.groupKeysMode);
    jsonable::toGeneric(j, "group_keys", req.groupKeys);
}


struct UpdateGroupUserBody {
    uint64_t gid;
    int mute {0xff};
    std::string nick {""};
    std::string nickname {""};
    std::string profileKeys {""};
    std::string groupNickname {""};
};

inline void to_json(nlohmann::json& j, const UpdateGroupUserBody& res)
{
    j = nlohmann::json{{"gid", res.gid},
                    {"mute", res.mute},
                    {"nickname", res.nickname},
                    {"group_nickname", res.groupNickname},
                    {"profile_keys", res.profileKeys},
                    {"nick", res.nick}
                    };
}

inline void from_json(const nlohmann::json& j, UpdateGroupUserBody& res)
{
    jsonable::toNumber(j, "gid", res.gid);
    jsonable::toNumber(j, "mute", res.mute, jsonable::OPTIONAL);
    jsonable::toString(j, "nick", res.nick, jsonable::OPTIONAL);
    jsonable::toString(j, "nickname", res.nickname, jsonable::OPTIONAL);
    jsonable::toString(j, "group_nickname", res.groupNickname, jsonable::OPTIONAL);
    jsonable::toString(j, "profile_keys", res.profileKeys, jsonable::OPTIONAL);
}

struct UpdateGroupBody {
	uint64_t gid;
	std::string notice;
};

inline void to_json(nlohmann::json& j, const UpdateGroupBody& res)
{
    j = nlohmann::json{{"gid", res.gid},
    				   {"notice", res.notice}
					};
}

inline void from_json(const nlohmann::json& j, UpdateGroupBody& res)
{
    jsonable::toNumber(j, "gid", res.gid);
    jsonable::toString(j, "notice", res.notice);
}

struct QueryGroupMemberInfo {
	uint64_t gid;
	std::string uid;
};

inline void to_json(nlohmann::json& j, const QueryGroupMemberInfo& res)
{
    j = nlohmann::json{{"gid", res.gid},
					{"uid", res.uid}
					};
}

inline void from_json(const nlohmann::json& j, QueryGroupMemberInfo& res)
{
    jsonable::toNumber(j, "gid", res.gid);
	jsonable::toString(j, "uid", res.uid);
}

struct QueryGroupMemberList {
	uint64_t gid;
	std::vector<int> role;
};

inline void to_json(nlohmann::json& j, const QueryGroupMemberList& res)
{
    j = nlohmann::json{{"gid", res.gid},
					{"role", res.role}
					};
}

inline void from_json(const nlohmann::json& j, QueryGroupMemberList& res)
{
    jsonable::toNumber(j, "gid", res.gid);
	jsonable::toGeneric(j, "role", res.role);
}

struct QueryGroupMemberListSeg {
    uint64_t gid;
    std::vector<int> role;
    std::string startUid;
    int count;
};

inline void to_json(nlohmann::json& j, const QueryGroupMemberListSeg& arg)
{
    boost::ignore_unused(j, arg);
}

inline void from_json(const nlohmann::json& j, QueryGroupMemberListSeg& arg)
{
    jsonable::toNumber(j, "gid", arg.gid);
    jsonable::toGeneric(j, "role", arg.role);
    jsonable::toString(j, "start_uid", arg.startUid, jsonable::OPTIONAL);
    jsonable::toNumber(j, "count", arg.count, jsonable::OPTIONAL);
}

struct UpdateGroupInfoBody {
    uint64_t gid {0};
    std::string name;
    std::string icon;
    std::string intro;
    int32_t broadcast{-1};
    std::string plainChannelKey;

    uint64_t updateTime{0};

    bool nameExisted{false};
    bool iconExisted{false};

    bool check(GroupResponse& response);
};

inline void to_json(nlohmann::json& j, const UpdateGroupInfoBody& body)
{
    if (body.nameExisted) {
        j["name"] = body.name;
    }

    if (body.iconExisted) {
        j["icon"] = body.icon;
    }

    if (!body.intro.empty()) {
        j["intro"] = body.intro;
    }

    if (body.broadcast == 0) {
        j["broadcast"] = body.broadcast;
    }

    if (!body.plainChannelKey.empty()) {
        j["plain_channel_key"] = body.plainChannelKey;
    }

    j["update_time"] = body.updateTime;
}

inline void from_json(const nlohmann::json& j, UpdateGroupInfoBody& body)
{
    body.nameExisted = (j.find("name") != j.end());
    body.iconExisted = (j.find("icon") != j.end());
    jsonable::toNumber(j, "gid", body.gid);
    jsonable::toString(j, "name", body.name, jsonable::OPTIONAL);
    jsonable::toString(j, "icon", body.icon, jsonable::OPTIONAL);
    jsonable::toString(j, "intro", body.intro, jsonable::OPTIONAL);
    jsonable::toNumber(j, "broadcast", body.broadcast, jsonable::OPTIONAL);
    jsonable::toString(j, "plain_channel_key", body.plainChannelKey, jsonable::OPTIONAL);
}

struct UpdateGroupInfoBodyV2 {
    uint64_t gid {0};
    std::string name;
    std::string icon;
    std::string intro;
    int32_t broadcast{-1};
    std::string plainChannelKey;

    uint64_t updateTime{0};

    bool nameExisted{false};
    bool iconExisted{false};
    std::string qrCodeSetting;
    int32_t ownerConfirm{-1};
    std::string shareSignature;
    std::string shareAndOwnerConfirmSignature;

    bool encryptedNameExisted{false};
    bool encryptedIconExisted{false};
    bool encryptedNoticeExisted{false};
    std::string encryptedName;
    std::string encryptedIcon;
    std::string encryptedNotice;

    bool check(GroupResponse& response);
};

inline void to_json(nlohmann::json& j, const UpdateGroupInfoBodyV2& body)
{
    if (body.nameExisted) {
        j["name"] = body.name;
    }

    if (body.iconExisted) {
        j["icon"] = body.icon;
    }

    if (!body.intro.empty()) {
        j["intro"] = body.intro;
    }

    if (body.broadcast == 0) {
        j["broadcast"] = body.broadcast;
    }

    if (!body.plainChannelKey.empty()) {
        j["plain_channel_key"] = body.plainChannelKey;
    }

    if (!body.qrCodeSetting.empty()) {
        j["share_qr_code_setting"] = body.qrCodeSetting;
    }

    if (body.ownerConfirm != -1) {
        j["owner_confirm"] = body.ownerConfirm;
    }

    if (!body.shareSignature.empty()) {
        j["share_sig"] = body.shareSignature;
    }

    if (!body.shareAndOwnerConfirmSignature.empty()) {
        j["share_and_owner_confirm_sig"] = body.shareAndOwnerConfirmSignature;
    }

    if (body.encryptedNameExisted) {
        j["encrypted_name"] = body.encryptedName;
    }

    if (body.encryptedIconExisted) {
        j["encrypted_icon"] = body.encryptedIcon;
    }

    if (body.encryptedNoticeExisted) {
        j["encrypted_notice"] = body.encryptedNotice;
    }

    j["update_time"] = body.updateTime;
}

inline void from_json(const nlohmann::json& j, UpdateGroupInfoBodyV2& body)
{
    body.nameExisted = (j.find("name") != j.end());
    body.iconExisted = (j.find("icon") != j.end());
    jsonable::toNumber(j, "gid", body.gid);
    jsonable::toString(j, "name", body.name, jsonable::OPTIONAL);
    jsonable::toString(j, "icon", body.icon, jsonable::OPTIONAL);
    jsonable::toString(j, "intro", body.intro, jsonable::OPTIONAL);
    jsonable::toNumber(j, "broadcast", body.broadcast, jsonable::OPTIONAL);
    jsonable::toString(j, "plain_channel_key", body.plainChannelKey, jsonable::OPTIONAL);
    jsonable::toString(j, "share_qr_code_setting", body.qrCodeSetting, jsonable::OPTIONAL);
    jsonable::toNumber(j, "owner_confirm", body.ownerConfirm, jsonable::OPTIONAL);
    jsonable::toString(j, "share_sig", body.shareSignature, jsonable::OPTIONAL);
    jsonable::toString(j, "share_and_owner_confirm_sig", body.shareAndOwnerConfirmSignature, jsonable::OPTIONAL);

    body.encryptedNameExisted = (j.find("encrypted_name") != j.end());
    body.encryptedIconExisted = (j.find("encrypted_icon") != j.end());
    body.encryptedNoticeExisted = (j.find("encrypted_notice") != j.end());
    jsonable::toString(j, "encrypted_name", body.encryptedName, jsonable::OPTIONAL);
    jsonable::toString(j, "encrypted_icon", body.encryptedIcon, jsonable::OPTIONAL);
    jsonable::toString(j, "encrypted_notice", body.encryptedNotice, jsonable::OPTIONAL);
}

struct UpdateGroupInfoBodyV3 {
    uint64_t gid {0};
    std::string name; // TODO: deprecated
    std::string icon; // TODO: deprecated
    std::string intro;
    int32_t broadcast{-1};
    std::string plainChannelKey;

    uint64_t updateTime{0};

    bool nameExisted{false};
    bool iconExisted{false};
    std::string qrCodeSetting;
    int32_t ownerConfirm{-1};
    std::string shareSignature;
    std::string shareAndOwnerConfirmSignature;
    bool encryptedGroupInfoSecretExisted{false};
    bool encryptedEphemeralKeyExisted{false};
    std::string encryptedGroupInfoSecret;
    std::string encryptedEphemeralKey;

    bool encryptedNameExisted{false};
    bool encryptedIconExisted{false};
    bool encryptedNoticeExisted{false};
    std::string encryptedName;
    std::string encryptedIcon;
    std::string encryptedNotice;

    bool check(GroupResponse& response);
};

inline void to_json(nlohmann::json& j, const UpdateGroupInfoBodyV3& body)
{
    if (body.nameExisted) {
        j["name"] = body.name;
    }

    if (body.iconExisted) {
        j["icon"] = body.icon;
    }

    if (!body.intro.empty()) {
        j["intro"] = body.intro;
    }

    if (body.broadcast == 0) {
        j["broadcast"] = body.broadcast;
    }

    if (!body.plainChannelKey.empty()) {
        j["plain_channel_key"] = body.plainChannelKey;
    }

    if (!body.qrCodeSetting.empty()) {
        j["share_qr_code_setting"] = body.qrCodeSetting;
    }

    if (body.ownerConfirm != -1) {
        j["owner_confirm"] = body.ownerConfirm;
    }

    if (!body.shareSignature.empty()) {
        j["share_sig"] = body.shareSignature;
    }

    if (!body.shareAndOwnerConfirmSignature.empty()) {
        j["share_and_owner_confirm_sig"] = body.shareAndOwnerConfirmSignature;
    }

    if (body.encryptedGroupInfoSecretExisted) {
        j["encrypted_group_info_secret"] = body.encryptedGroupInfoSecret;
    }

    if (body.encryptedEphemeralKeyExisted) {
        j["encrypted_ephemeral_key"] = body.encryptedEphemeralKey;
    }

    if (body.encryptedNameExisted) {
        j["encrypted_name"] = body.encryptedName;
    }

    if (body.encryptedIconExisted) {
        j["encrypted_icon"] = body.encryptedIcon;
    }

    if (body.encryptedNoticeExisted) {
        j["encrypted_notice"] = body.encryptedNotice;
    }

    j["update_time"] = body.updateTime;
}

inline void from_json(const nlohmann::json& j, UpdateGroupInfoBodyV3& body)
{
    body.nameExisted = (j.find("name") != j.end());
    body.iconExisted = (j.find("icon") != j.end());
    body.encryptedGroupInfoSecretExisted = (j.find("encrypted_group_info_secret") != j.end());
    body.encryptedEphemeralKeyExisted = (j.find("encrypted_ephemeral_key") != j.end());
    jsonable::toNumber(j, "gid", body.gid);
    jsonable::toString(j, "name", body.name, jsonable::OPTIONAL);
    jsonable::toString(j, "icon", body.icon, jsonable::OPTIONAL);
    jsonable::toString(j, "intro", body.intro, jsonable::OPTIONAL);
    jsonable::toNumber(j, "broadcast", body.broadcast, jsonable::OPTIONAL);
    jsonable::toString(j, "plain_channel_key", body.plainChannelKey, jsonable::OPTIONAL);
    jsonable::toString(j, "share_qr_code_setting", body.qrCodeSetting, jsonable::OPTIONAL);
    jsonable::toNumber(j, "owner_confirm", body.ownerConfirm, jsonable::OPTIONAL);
    jsonable::toString(j, "share_sig", body.shareSignature, jsonable::OPTIONAL);
    jsonable::toString(j, "share_and_owner_confirm_sig", body.shareAndOwnerConfirmSignature, jsonable::OPTIONAL);
    jsonable::toString(j, "encrypted_group_info_secret", body.encryptedGroupInfoSecret, jsonable::OPTIONAL);
    jsonable::toString(j, "encrypted_ephemeral_key", body.encryptedEphemeralKey, jsonable::OPTIONAL);

    body.encryptedNameExisted = (j.find("encrypted_name") != j.end());
    body.encryptedIconExisted = (j.find("encrypted_icon") != j.end());
    body.encryptedNoticeExisted = (j.find("encrypted_notice") != j.end());
    jsonable::toString(j, "encrypted_name", body.encryptedName, jsonable::OPTIONAL);
    jsonable::toString(j, "encrypted_icon", body.encryptedIcon, jsonable::OPTIONAL);
    jsonable::toString(j, "encrypted_notice", body.encryptedNotice, jsonable::OPTIONAL);

}

inline void to_json(nlohmann::json& j, const Group& group)
{
    j = nlohmann::json{{"gid", group.gid()},
                       {"name", group.name()}, // TODO: deprecated
                       {"icon", group.icon()}, // TODO: deprecated
                       {"permission", group.permission()},
                       {"update_time", group.updatetime()},
                       {"last_mid", group.lastmid()},
                       {"broadcast", group.broadcast()},
                       {"status", group.status()},
                       {"channel", group.channel()},
                       {"intro", group.intro()},
                       {"create_time", group.createtime()},
                       {"encrypted", static_cast<int32_t>(group.encryptstatus())},
                       {"plain_channel_key", group.plainchannelkey()},
                       {"notice", jsonable::safe_parse(group.notice())}, // TODO: deprecated
                       {"share_qr_code_setting", group.shareqrcodesetting()},
                       {"owner_confirm", group.ownerconfirm()},
                       {"share_sig", group.sharesignature()},
                       {"share_and_owner_confirm_sig", group.shareandownerconfirmsignature()},
                       {"encrypted_name", group.encryptedname()},
                       {"encrypted_icon", group.encryptedicon()},
                       {"encrypted_notice", group.encryptednotice()}};
}

struct QueryGroupInfoBody {
    uint64_t gid{0};

    bool check(GroupResponse& response);
};

inline void from_json(const nlohmann::json& j, QueryGroupInfoBody& body)
{
    jsonable::toNumber(j, "gid", body.gid, jsonable::OPTIONAL);
}

inline void to_json(nlohmann::json&, const QueryGroupInfoBody&)
{

}

struct QueryGroupInfoByGidBatch {
    std::vector<uint64_t> gids;
};

inline void from_json(const nlohmann::json& j, QueryGroupInfoByGidBatch& arg)
{
    jsonable::toGeneric(j, "gids", arg.gids);
}

inline void to_json(nlohmann::json&, const QueryGroupInfoByGidBatch&)
{
}

struct InviteGroupMemberBodyV2 {
    uint64_t gid;
    std::vector<std::string> members;
    std::vector<std::string> memberGroupInfoSecrets;
    std::vector<std::string> memberKeys;
    std::vector<std::string> signatureInfos;

    bool check(GroupResponse &response);
};


inline void from_json(const nlohmann::json& j, InviteGroupMemberBodyV2& body)
{
    jsonable::toNumber(j, "gid", body.gid);
    jsonable::toGeneric(j, "members", body.members);
    jsonable::toGeneric(j, "group_info_secrets", body.memberGroupInfoSecrets, jsonable::OPTIONAL);
    jsonable::toGeneric(j, "member_keys", body.memberKeys, jsonable::OPTIONAL);
    jsonable::toGeneric(j, "signature", body.signatureInfos, jsonable::OPTIONAL);

}

inline void to_json(nlohmann::json&, const InviteGroupMemberBodyV2&)
{

}

 struct InviteGroupMemberResponse2 {
     std::vector<SimpleGroupMemberInfo> successMembers;
     std::vector<std::string> failedMembers;
 };

inline void to_json(nlohmann::json& j, const InviteGroupMemberResponse2& response)
{
    j = nlohmann::json{{"success_members", response.successMembers},
                       {"failed_members", response.failedMembers}};
}


struct KickGroupMemberBody {
    uint64_t gid;
    std::vector<std::string> members;

    bool check(GroupResponse& response, const std::string& uid);
};

inline void from_json(const nlohmann::json& j, KickGroupMemberBody& body)
{
    jsonable::toNumber(j, "gid", body.gid);
    jsonable::toGeneric(j, "members", body.members);
}

inline void to_json(nlohmann::json& j, const KickGroupMemberBody& body)
{
    j = nlohmann::json{{"gid", body.gid},
                       {"members", body.members}};
}

struct GetOwnerConfirmInfo {
    uint64_t gid{0};
};

inline void from_json(const nlohmann::json& j, GetOwnerConfirmInfo& body)
{
    jsonable::toNumber(j, "gid", body.gid);

}

inline void to_json(nlohmann::json&, const GetOwnerConfirmInfo&)
{

}

struct QrCodeInfo {
    uint64_t gid;
    std::string qrCodeToken;
    bool check(std::string& error);
};

inline void from_json(const nlohmann::json& j, QrCodeInfo& req)
{
    jsonable::toNumber(j, "gid", req.gid);
    jsonable::toString(j, "signature", req.qrCodeToken);
}

inline void to_json(nlohmann::json &, const QrCodeInfo &)
{
}

struct LeaveGroupBody {
    uint64_t gid{0};
    std::string nextOwner;

    bool check(GroupResponse& response);
};

inline void from_json(const nlohmann::json& j, LeaveGroupBody& body)
{
    jsonable::toNumber(j, "gid", body.gid);
    jsonable::toString(j, "next_owner", body.nextOwner, jsonable::OPTIONAL);
}

inline void to_json(nlohmann::json&, const LeaveGroupBody&)
{

}

struct GroupUpdateGroupKeysRequestBody {
    int32_t groupKeysMode;
};

inline void to_json(nlohmann::json& j, const GroupUpdateGroupKeysRequestBody& body)
{
    j = nlohmann::json{{"group_keys_mode", body.groupKeysMode}};
}

struct GroupSwitchGroupKeysBody {
    int64_t version;
};

inline void to_json(nlohmann::json& j, const GroupSwitchGroupKeysBody& body)
{
    j = nlohmann::json{{"version", body.version}};
}

struct GroupSysMsgBody {
    GroupMemberUpdateAction action;
    std::vector<SimpleGroupMemberInfo> members;
};

inline void to_json(nlohmann::json& j, const GroupSysMsgBody& body)
{
    j = nlohmann::json{{"action", static_cast<int32_t>(body.action)},
                       {"members", body.members}};
}

struct GameQueryGroupInfoById {
    uint64_t gid{0};
};

inline void from_json(const nlohmann::json& j, GameQueryGroupInfoById& req)
{
    jsonable::toNumber(j, "gid", req.gid);
}

inline void to_json(nlohmann::json&, const GameQueryGroupInfoById&)
{
}

struct GroupJoinRequest {
    uint64_t gid;
    std::string qrCode;
    std::string qrCodeToken;
    std::string signature;
    std::string comment;

    bool check(std::string& error);
};

inline void from_json(const nlohmann::json& j, GroupJoinRequest& req)
{
    jsonable::toNumber(j, "gid", req.gid);
    jsonable::toString(j, "qr_code", req.qrCode);
    jsonable::toString(j, "qr_token", req.qrCodeToken);
    jsonable::toString(j, "signature", req.signature);
    jsonable::toString(j, "comment", req.comment);
}

inline void to_json(nlohmann::json&, const GroupJoinRequest&)
{
}

struct QueryGroupPendingListRequest {
    uint64_t gid;
    std::string startUid;
    int count;

    bool check(std::string& error);
};

inline void from_json(const nlohmann::json& j, QueryGroupPendingListRequest& req)
{
    jsonable::toNumber(j, "gid", req.gid);
    jsonable::toString(j, "start", req.startUid);
    jsonable::toNumber(j, "count", req.count);
}

inline void to_json(nlohmann::json&, const QueryGroupPendingListRequest&)
{
}

struct ReviewJoinResult {
    std::string uid;
    bool accepted;
    std::string groupSecret;
    std::string groupInfoSecret;
    std::string inviter;
};

inline void from_json(const nlohmann::json& j, ReviewJoinResult& req)
{
    jsonable::toString(j, "uid", req.uid);
    jsonable::toBoolean(j, "accepted", req.accepted);
    jsonable::toString(j, "pwd", req.groupSecret);
    jsonable::toString(j, "group_info_secret", req.groupInfoSecret);
    jsonable::toString(j, "inviter", req.inviter, jsonable::OPTIONAL);
}

inline void to_json(nlohmann::json&, const ReviewJoinResult&)
{
}

struct ReviewJoinResultList {
    uint64_t gid;
    std::vector<ReviewJoinResult> list;

    bool check(std::string& error);
};

inline void from_json(const nlohmann::json& j, ReviewJoinResultList& req)
{
    jsonable::toNumber(j, "gid", req.gid);
    jsonable::toGeneric(j, "list", req.list);
}

inline void to_json(nlohmann::json&, const ReviewJoinResultList&)
{
}

inline void to_json(nlohmann::json& j, const PendingGroupUser& user)
{
    j["gid"] = user.gid();
    j["uid"] = user.uid();
    j["inviter"] = user.inviter();
    j["signature"] = user.signature();
    j["comment"] = user.comment();
}

namespace dao {

inline void to_json(nlohmann::json& j, const UserGroupDetail& detail)
{
    j = detail.group;
    j["owner"] = detail.counter.owner;
    j["member_cn"] = detail.counter.memberCnt;
    j["subscriber_cn"] = detail.counter.subscriberCnt;
    j["version"] = detail.group.version();
    if (detail.user.role() != GroupUser::ROLE_UNDEFINE) {
        j["encrypted_key"] = detail.user.encryptedkey();
        j["group_info_secret"] = detail.user.groupinfosecret();
        j["last_ack_mid"] = detail.user.lastackmid();
        j["role"] = static_cast<int32_t>(detail.user.role());
        j["encrypted_ephemeral_key"] = detail.group.encryptedephemeralkey();
    } else {
        j["encrypted_key"] = "";
        j["last_ack_mid"] = 0;
        j["role"] = static_cast<int32_t>(detail.user.role());
    }
}

inline void to_json(nlohmann::json& j, const UserGroupEntry& entry)
{
    j = entry.group;
    j["owner"] = entry.owner;
    j["version"] = entry.group.version();
    if (entry.user.role() != GroupUser::ROLE_UNDEFINE) {
        j["encrypted_key"] = entry.user.encryptedkey();
        j["group_info_secret"] = entry.user.groupinfosecret();
        j["last_ack_mid"] = entry.user.lastackmid();
        j["role"] = static_cast<int32_t>(entry.user.role());
        j["encrypted_ephemeral_key"] = entry.group.encryptedephemeralkey();
    } else {
        j["encrypted_key"] = "";
        j["last_ack_mid"] = 0;
        j["role"] = static_cast<int32_t>(entry.user.role());
    }
}

} // namespace dao


struct QueryMemberListOrderedBody {
    uint64_t gid;
    std::vector<int> roles;
    std::string startUid;
    int64_t createTime{0};
    int count{0};

    bool check(std::string& error);
};

inline void to_json(nlohmann::json& j, const QueryMemberListOrderedBody& arg)
{
    boost::ignore_unused(j, arg);
}

inline void from_json(const nlohmann::json& j, QueryMemberListOrderedBody& arg)
{
    jsonable::toNumber(j, "gid", arg.gid);
    jsonable::toGeneric(j, "role", arg.roles);
    jsonable::toString(j, "startUid", arg.startUid, jsonable::OPTIONAL);
    jsonable::toNumber(j, "createTime", arg.createTime, jsonable::OPTIONAL);
    jsonable::toNumber(j, "count", arg.count, jsonable::OPTIONAL);
}

struct DhKeysRequest {
    std::set<std::string> uids;
    bool check();
};

inline void from_json(const nlohmann::json& j, DhKeysRequest& req)
{
    jsonable::toGeneric(j, "uids", req.uids);
}

inline void to_json(nlohmann::json& j, const DhKeysRequest& req)
{
    j["uids"] = req.uids;
}

struct DhKeysResponse {
    std::vector<bcm::Keys> keys;
};

inline void from_json(const nlohmann::json& j, bcm::OnetimeKey& key)
{
    boost::ignore_unused(j, key);
}

inline void from_json(const nlohmann::json& j, bcm::Keys& keys)
{
    boost::ignore_unused(j, keys);
}

inline void from_json(const nlohmann::json& j, DhKeysResponse& resp)
{
    boost::ignore_unused(j, resp);
}

inline void to_json(nlohmann::json& j, const bcm::OnetimeKey& key)
{
    j = nlohmann::json{{"keyId", key.keyid()},
                       {"publicKey", key.publickey()}};
}

inline void to_json(nlohmann::json& j, const bcm::Keys& keys)
{
    j["uid"] = keys.uid();
    j["device_id"] = keys.deviceid();
    j["registration_id"] = keys.registrationid();
    j["signed_prekey"] = keys.signedprekey();
    j["onetime_key"] = keys.onetimekey();
    j["identity_key"] = keys.identitykey();
    j["account_publickey"] = keys.accountpublickey();
    j["account_signature"] = keys.accountsignature();
}

inline void to_json(nlohmann::json& j, const DhKeysResponse& resp)
{
    j["keys"] = resp.keys;
}

struct PrepareKeyUpdateRequestV3 {
    uint64_t gid;
    int64_t version;
    int32_t mode;
};

inline void from_json(const nlohmann::json& j, PrepareKeyUpdateRequestV3& req)
{
    jsonable::toNumber(j, "gid", req.gid);
    jsonable::toNumber(j, "version", req.version);
    jsonable::toNumber(j, "mode", req.mode);
}

inline void to_json(nlohmann::json& j, const PrepareKeyUpdateRequestV3& req)
{
    j = nlohmann::json{{"gid", req.gid},
                       {"version", req.version},
                       {"mode", req.mode}
    };
}

struct PrepareKeyUpdateResponseV3 {
    std::vector<bcm::Keys> keys;
};

inline void from_json(const nlohmann::json& j, PrepareKeyUpdateResponseV3& resp)
{
    boost::ignore_unused(j, resp);
}

inline void to_json(nlohmann::json& j, const PrepareKeyUpdateResponseV3& resp)
{
    j["keys"] = resp.keys;
}

struct GroupJoinRequestV3 {
    uint64_t gid;
    std::string qrCode;
    std::string qrCodeToken;
    std::string signature;
    std::string comment;

    bool check();
};

inline void from_json(const nlohmann::json& j, GroupJoinRequestV3& req)
{
    jsonable::toNumber(j, "gid", req.gid);
    jsonable::toString(j, "qr_code", req.qrCode);
    jsonable::toString(j, "qr_token", req.qrCodeToken);
    jsonable::toString(j, "signature", req.signature);
    jsonable::toString(j, "comment", req.comment);
}

inline void to_json(nlohmann::json&, const GroupJoinRequestV3&)
{
}

struct GroupJoinResponseV3 {
    bool ownerConfirm;
    std::string encryptedGroupInfoSecret;
};

inline void from_json(const nlohmann::json&, GroupJoinResponseV3&)
{
    
}

inline void to_json(nlohmann::json& j, const GroupJoinResponseV3& resp)
{
    j["owner_confirm"] = resp.ownerConfirm;
    j["encrypted_group_info_secret"] = resp.encryptedGroupInfoSecret;
}

struct AddMeRequestV3 {
    uint64_t gid;
    std::string groupInfoSecret;
    std::string proof;

    bool check();
};

inline void from_json(const nlohmann::json& j, AddMeRequestV3& req)
{
    jsonable::toNumber(j, "gid", req.gid);
    jsonable::toString(j, "group_info_secret", req.groupInfoSecret);
    jsonable::toString(j, "proof", req.proof);
}

inline void to_json(nlohmann::json&, const AddMeRequestV3&)
{
}

struct InviteGroupMemberRequestV3 {
    uint64_t gid;
    std::vector<std::string> members;
    std::vector<std::string> memberGroupInfoSecrets;
    std::vector<std::string> memberProofs;
    std::vector<std::string> signatureInfos;

    bool check();
};

inline void from_json(const nlohmann::json& j, InviteGroupMemberRequestV3& body)
{
    jsonable::toNumber(j, "gid", body.gid);
    jsonable::toGeneric(j, "members", body.members);
    jsonable::toGeneric(j, "group_info_secrets", body.memberGroupInfoSecrets, jsonable::OPTIONAL);
    jsonable::toGeneric(j, "member_proofs", body.memberProofs, jsonable::OPTIONAL);
    jsonable::toGeneric(j, "signature", body.signatureInfos, jsonable::OPTIONAL);

}

inline void to_json(nlohmann::json&, const InviteGroupMemberRequestV3&)
{

}

typedef InviteGroupMemberResponse2 InviteGroupMemberResponseV3;

inline void from_json(const nlohmann::json&, InviteGroupMemberResponseV3&)
{
    
}

struct ReviewJoinResultV3 {
    std::string uid;
    bool accepted;
    std::string groupInfoSecret;
    std::string inviter;
    std::string proof;
};

inline void from_json(const nlohmann::json& j, ReviewJoinResultV3& req)
{
    jsonable::toString(j, "uid", req.uid);
    jsonable::toBoolean(j, "accepted", req.accepted);
    jsonable::toString(j, "group_info_secret", req.groupInfoSecret);
    jsonable::toString(j, "inviter", req.inviter, jsonable::OPTIONAL);
    jsonable::toString(j, "proof", req.proof);
}

inline void to_json(nlohmann::json&, const ReviewJoinResultV3&)
{
}

struct ReviewJoinResultRequestV3 {
    uint64_t gid;
    std::vector<ReviewJoinResultV3> list;

    bool check();
};

inline void from_json(const nlohmann::json& j, ReviewJoinResultRequestV3& req)
{
    jsonable::toNumber(j, "gid", req.gid);
    jsonable::toGeneric(j, "list", req.list);
}

inline void to_json(nlohmann::json&, const ReviewJoinResultRequestV3&)
{
}

struct QueryMembersRequestV3 {
    uint64_t gid;
    std::vector<std::string> uids;

    bool check();
};

inline void from_json(const nlohmann::json& j, QueryMembersRequestV3& req)
{
    jsonable::toNumber(j, "gid", req.gid);
    jsonable::toGeneric(j, "uids", req.uids);
}

inline void to_json(nlohmann::json&, const QueryMembersRequestV3&)
{
}

struct GroupMember {
    std::string uid;
    std::string nick;
    std::string nickname;
    std::string groupNickname;
    std::string profileKeys;
    int32_t role;
    uint64_t createTime;
    std::string proof;
};

inline void from_json(const nlohmann::json&, GroupMember&)
{
}

inline void to_json(nlohmann::json& j, const GroupMember& m)
{
    j["uid"] = m.uid;
    j["nick"] = m.nick;
    j["nickname"] = m.nickname;
    j["groupNickname"] = m.groupNickname;
    j["profileKeys"] = m.profileKeys;
    j["role"] = m.role;
    j["createTime"] = m.createTime;
    j["proof"] = m.proof;
}

struct QueryMembersResponseV3 {
    std::vector<GroupMember> members;
};

inline void from_json(const nlohmann::json&, QueryMembersResponseV3&)
{
}

inline void to_json(nlohmann::json& j, const QueryMembersResponseV3& resp)
{
    j["members"] = resp.members;
}

struct GroupExtensionInfo {
    uint64_t gid;
    std::map<std::string, std::string> extensions;
};

inline void from_json(const nlohmann::json& j, GroupExtensionInfo& body)
{
    jsonable::toNumber(j, "gid", body.gid);
    jsonable::toGeneric(j, "extensions", body.extensions);
}

inline void to_json(nlohmann::json& j, const GroupExtensionInfo& body)
{
    j = nlohmann::json{{"gid", body.gid},
                       {"extensions", body.extensions}
    };
}

struct QueryGroupExtensionInfo {
    uint64_t gid;
    std::set<std::string> extensionKeys;
};

inline void from_json(const nlohmann::json& j, QueryGroupExtensionInfo& body)
{
    jsonable::toNumber(j, "gid", body.gid);
    jsonable::toGeneric(j, "extensionKeys", body.extensionKeys);
}

inline void to_json(nlohmann::json& j, const QueryGroupExtensionInfo& body)
{
    j = nlohmann::json{{"gid", body.gid},
                       {"extensionKeys", body.extensionKeys}
    };
}


} // namespace bcm

