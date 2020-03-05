#pragma once

#include <string>
#include <vector>
#include <map>
#include <utils/jsonable.h>
#include <utils/log.h>


namespace bcm {

struct ContactTokens {
    std::vector<std::string> contacts;
};

inline void to_json(nlohmann::json& j, const ContactTokens& tokens)
{
    j = nlohmann::json{{"contacts", tokens.contacts}};
}

inline void from_json(const nlohmann::json& j, ContactTokens& tokens)
{
    jsonable::toGeneric(j, "contacts", tokens.contacts);
}

struct ContactResponse {
    std::map<std::string, std::vector<std::string>> contacts{};
};

inline void to_json(nlohmann::json& j, const ContactResponse& uids)
{
    j = nlohmann::json{{"contacts", uids.contacts}};
}

inline void from_json(const nlohmann::json& j, ContactResponse& uids)
{
    jsonable::toGeneric(j, "contacts", uids.contacts);
}

/*
{
    "parts": {
            "0": hash_of_part_0_contacts,
            "1": hash_of_part_1_contacts,
            "2": hash_of_part_2_contacts
    }
*/
struct ContactInPartsReq {
    std::map<std::string, uint32_t> hash;
};

inline void to_json(nlohmann::json& j, const ContactInPartsReq& hashInPart)
{
    j = nlohmann::json{{"parts", hashInPart.hash}};
}

inline void from_json(const nlohmann::json& j, ContactInPartsReq& hashInPart)
{
    jsonable::toGeneric(j, "parts", hashInPart.hash);
}

/*
{
    "parts": {
            "0": "contacts_of_part_0",
            "1": "contacts_of_part_1"
        }
}
*/
struct ContactInParts {
    std::map<std::string, std::string> contacts;
};

inline void to_json(nlohmann::json& j, const ContactInParts& c)
{
    j["parts"] = c.contacts;
}

inline void from_json(const nlohmann::json& j, ContactInParts& c)
{
    jsonable::toGeneric(j, "parts", c.contacts);
}

/*
{
    "tokens":[
        "11111",
        "22222",
        "33333"
    ],
    "needUsers":1
}
*/
struct UploadTokensReq {
    std::vector<std::string> tokens;
    int needUsers;
};

inline void to_json(nlohmann::json& j, const UploadTokensReq& tokens)
{
    j["tokens"] = tokens.tokens;
    j["needUsers"] = tokens.needUsers;
}

inline void from_json(const nlohmann::json& j, UploadTokensReq& tokens)
{
    jsonable::toGeneric(j, "tokens", tokens.tokens);
    jsonable::toNumber(j, "needUsers", tokens.needUsers);
}

/*
{
    "hash":hash_of_uploaded_tokens
    "contacts": {
        "token1": [
            "UID1_of_token1",
            "UID2_of_token1"
       ],
        "token2": [
            "UID1_of_token2",
            "UID2_of_token2"
       ]
   }
}
*/
struct ContactResponseWithHash : public ContactResponse{
    uint32_t hash{0};
};

inline void to_json(nlohmann::json&j, const ContactResponseWithHash& c)
{
    j["hash"] = c.hash;
    j["contacts"] = c.contacts;
}

inline void from_json(const nlohmann::json& j, ContactResponseWithHash& c)
{
    jsonable::toNumber(j, "hash", c.hash);
    jsonable::toGeneric(j, "contacts", c.contacts);
}

struct ContactsFiltersPutReq {
    uint32_t algo{0};
    std::string content;
};
struct ContactsFiltersPutResp {
    std::string version;
};

inline void to_json(nlohmann::json&, const ContactsFiltersPutReq&)
{
}

inline void from_json(const nlohmann::json& j, ContactsFiltersPutReq& req)
{
    jsonable::toNumber(j, "algo", req.algo);
    jsonable::toString(j, "content", req.content);
}

struct FiltersPatch {
    uint32_t position{0};
    bool value{false};
};

inline void to_json(nlohmann::json&, const FiltersPatch&)
{
}

inline void from_json(const nlohmann::json& j, FiltersPatch& patch)
{
    jsonable::toNumber(j, "position", patch.position);
    jsonable::toBoolean(j, "value", patch.value);
}

struct ContactsFiltersPatchReq {
    std::string version;
    std::vector<FiltersPatch> patches;
};

inline void to_json(nlohmann::json&, const ContactsFiltersPatchReq&)
{
}

inline void from_json(const nlohmann::json& j, ContactsFiltersPatchReq& req)
{
    jsonable::toString(j, "version", req.version);
    jsonable::toGeneric(j, "patches", req.patches);
}

struct ContactsFiltersVersion {
    std::string version;
};

inline void to_json(nlohmann::json& j, const ContactsFiltersVersion& version)
{
    j["version"] = version.version;
}

inline void from_json(const nlohmann::json&, ContactsFiltersVersion&)
{
}

struct FriendRequestParams {
    std::string target;
    uint64_t timestamp{0};
    std::string payload;
    std::string signature;
};

inline void to_json(nlohmann::json&, const FriendRequestParams&)
{
}

inline void from_json(const nlohmann::json& j, FriendRequestParams& params)
{
    jsonable::toString(j, "target", params.target);
    jsonable::toNumber(j, "timestamp", params.timestamp);
    jsonable::toString(j, "payload", params.payload);
    jsonable::toString(j, "signature", params.signature);
}

struct FriendReplyParams {
    bool approved{false};
    std::string proposer;
    uint64_t timestamp{0};
    std::string payload;
    std::string signature;
    std::string requestSignature;
};

inline void to_json(nlohmann::json&, const FriendReplyParams&)
{
}

inline void from_json(const nlohmann::json& j, FriendReplyParams& params)
{
    jsonable::toBoolean(j, "approved", params.approved);
    jsonable::toString(j, "proposer", params.proposer);
    jsonable::toNumber(j, "timestamp", params.timestamp);
    jsonable::toString(j, "payload", params.payload);
    jsonable::toString(j, "signature", params.signature);
    jsonable::toString(j, "requestSignature", params.requestSignature);
}

struct DeleteFriendParams {
    std::string target;
    uint64_t timestamp{0};
    std::string signature;
};

inline void to_json(nlohmann::json&, const DeleteFriendParams&)
{
}

inline void from_json(const nlohmann::json& j, DeleteFriendParams& params)
{
    jsonable::toString(j, "target", params.target);
    jsonable::toNumber(j, "timestamp", params.timestamp);
    jsonable::toString(j, "signature", params.signature);
}

} // namespace bcm
