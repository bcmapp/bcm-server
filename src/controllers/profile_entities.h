#pragma once

#include "utils/jsonable.h"
#include "proto/dao/account.pb.h"

namespace bcm {

void to_json(nlohmann::json& j, const Account::Privacy& privacy)
{
    j = nlohmann::json {{"stranger", privacy.acceptstrangermsg()}};
}

void from_json(const nlohmann::json& j, Account::Privacy& privacy)
{
    bool acceptStranger = true;

    jsonable::toBoolean(j, "stranger", acceptStranger);

    privacy.set_acceptstrangermsg(acceptStranger);
}

struct Profile {
    // deprecated
    std::string name;
    std::string avatar;
    std::string phoneNum;
    std::string encryptNumber;
    std::string numberPubkey;

    std::string identityKey;
    std::string nickname;
    std::string ldAvatar;
    std::string hdAvatar;
    bool supportVoice{false};
    bool supportVideo{false};
    Account::Privacy privacy;
    Account::State state{Account::NORMAL};
    std::string features;
};

void to_json(nlohmann::json& j, const Profile& profile)
{
    j = nlohmann::json{
        {"identityKey", profile.identityKey},
        {"name", profile.name},
        {"avatar", profile.avatar},
        {"nickname", profile.nickname},
        {"ldAvatar", profile.ldAvatar},
        {"hdAvatar", profile.hdAvatar},
        {"voice", profile.supportVoice},
        {"video", profile.supportVideo},
        {"number", profile.phoneNum},
        {"encryptNumber", profile.encryptNumber},
        {"numberPubkey", profile.numberPubkey},
        {"privacy", profile.privacy},
        {"state", profile.state},
        {"features", profile.features}
    };
}

void from_json(const nlohmann::json&, Profile&)
{
}

struct ProfileMap {
    std::map<std::string, Profile> profileMap;
};

void to_json(nlohmann::json& j, const ProfileMap& profileMap)
{
    j = nlohmann::json{
        {"profiles", profileMap.profileMap},
    };
}

void from_json(const nlohmann::json&, ProfileMap&)
{
}


struct AvatarId {
    std::string avatarId;
};

void to_json(nlohmann::json& j, const AvatarId& avatarId)
{
    j = nlohmann::json{
        {"avatarNamePlaintext", avatarId.avatarId},
    };
}

void from_json(const nlohmann::json&, AvatarId&)
{
}

struct UrlResult {
    std::string location;
};

void to_json(nlohmann::json& j, const UrlResult& url)
{
    j = nlohmann::json{
        {"location", url.location}
    };
}

void from_json(const nlohmann::json&, UrlResult&)
{
}

struct GroupAvatarResult {
    uint32_t code{0};
    std::string msg;
    UrlResult url;
};

void to_json(nlohmann::json& j, const GroupAvatarResult& groupAvatar)
{
    j = nlohmann::json{
        {"error_code", groupAvatar.code},
        {"error_msg", groupAvatar.msg},
        {"result", groupAvatar.url}
    };
}

void from_json(const nlohmann::json&, GroupAvatarResult&)
{
}

struct AvatarBundle {
    std::string ldAvatar;
    std::string hdAvatar;
};

void to_json(nlohmann::json& j, const AvatarBundle& avatarBundle)
{
    j = nlohmann::json{
            {"ldAvatar", avatarBundle.ldAvatar},
            {"hdAvatar", avatarBundle.hdAvatar}
    };
}

void from_json(const nlohmann::json& j, AvatarBundle& avatarBundle)
{
    jsonable::toString(j, "ldAvatar", avatarBundle.ldAvatar);
    jsonable::toString(j, "hdAvatar", avatarBundle.hdAvatar);
}

} // namespace bcm
