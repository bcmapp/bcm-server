#pragma once

#include <utils/jsonable.h>
#include "offline_server_config.h"

namespace bcm {
    // redisdb hash key
    const std::string REDISDB_KEY_PREFIX_GROUP_USER_INFO = "group_user_msg_";
    const std::string REDISDB_KEY_GROUP_MSG_INFO = "group_msg_list";
    const std::string REDISDB_KEY_GROUP_MULTI_LIST_INFO = "group_multi_msg_list";
    const std::string REDISDB_KEY_GROUP_REDIS_ACTIVE = "group_msg_active";
    const std::string REDISDB_KEY_APNS_UID_BADGE_PREFIX = "apns_badge_";

    enum PushPeopleType {
        PUSHPEOPLETYPE_UNKNOWN = 0,
        PUSHPEOPLETYPE_TO_ALL = 1,
        PUSHPEOPLETYPE_TO_DESIGNATED_PERSON = 2,
    };
    
    enum GroupUserConfigPushType {
        NORMAL = 0,
        NO_CONFIG = 1
    };
    
    struct GroupUserMessageIdInfo {
        uint64_t last_mid{0};
        // google cloud messaging
        std::string gcmId{""};
        // U meng
        std::string umengId{""};
        uint32_t    osType{0};
        std::string osVersion{""};
        uint64_t    bcmBuildCode{0};
        std::string phoneModel{""};

        // apple push notification service
        std::string apnId{""};
        std::string apnType{""};
        std::string voipApnId{""};

        std::string targetAddress{""};

        //
        int32_t  cfgFlag{GroupUserConfigPushType::NORMAL};

        std::string to_string()
        {
            nlohmann::json j;
            j["last_mid"] = last_mid;
            j["cfgFlag"] = cfgFlag;

            j["gcmId"] = gcmId;

            j["umengId"] = umengId;
            j["osType"] = osType;
            j["osVersion"] = osVersion;
            j["bcmBuildCode"] = bcmBuildCode;
            j["phoneModel"] = phoneModel;

            j["apnId"] = apnId;
            j["apnType"] = apnType;
            j["voipApnId"] = voipApnId;

            j["targetAddress"] =  targetAddress;

            return j.dump();
        }

        bool from_string(const std::string& str)
        {
            try {
                nlohmann::json j = nlohmann::json::parse(str);

                jsonable::toNumber(j, "last_mid", last_mid, jsonable::OPTIONAL);
                jsonable::toNumber(j, "cfgFlag", cfgFlag, jsonable::OPTIONAL);

                if (j.find("gcmId") != j.end()) {
                    jsonable::toString(j, "gcmId", gcmId);
                }

                if (j.find("umengId") != j.end()) {
                    jsonable::toString(j, "umengId", umengId);
                }

                if (j.find("apnId") != j.end()) {
                    jsonable::toString(j, "apnId", apnId);
                    jsonable::toString(j, "apnType", apnType);
                    jsonable::toString(j, "voipApnId", voipApnId);
                }

                jsonable::toNumber(j, "osType", osType, jsonable::OPTIONAL);
                jsonable::toNumber(j, "bcmBuildCode", bcmBuildCode, jsonable::OPTIONAL);
                jsonable::toString(j, "phoneModel", phoneModel, jsonable::OPTIONAL);
                jsonable::toString(j, "osVersion", osVersion, jsonable::OPTIONAL);

                jsonable::toString(j, "targetAddress", targetAddress, jsonable::OPTIONAL);
            } catch (std::exception& e) {
                return false;
            }

            return true;
        }

    };

    // when type = GroupMsg::TYPE_MEMBER_UPDATE
    struct GroupMultibroadMessageInfo {
        std::set<std::string> members;
        std::string from_uid{""};

        std::string to_string()
        {
            nlohmann::json j;
            j["members"] = members;
            
            if ("" != from_uid) {
                j["from_uid"] = from_uid;
            }
            
            return j.dump();
        }
        
        bool from_string(const std::string& str)
        {
            try {
                nlohmann::json j = nlohmann::json::parse(str);
    
                jsonable::toGeneric(j, "members", members, jsonable::REQUIRE);
                jsonable::toString(j, "from_uid", from_uid, jsonable::OPTIONAL);

            } catch (std::exception& e) {
                return false;
            }
            
            return true;
        }
    };
    
    
}
