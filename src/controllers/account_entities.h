#pragma once

#include <utils/jsonable.h>
#include <proto/dao/sign_up_challenge.pb.h>
#include <proto/dao/client_version.pb.h>

namespace bcm {

//message SignUpChallenge {
//    uint32 difficulty = 1;
//    int64 nonce = 2;
//    uint64 timestamp = 3;
//}

inline void to_json(nlohmann::json& j, const SignUpChallenge& challenge)
{
    j = nlohmann::json {{"difficulty", challenge.difficulty()},
                        {"nonce", challenge.nonce()},
                        {"timestamp", challenge.timestamp()}
    };
}

inline void from_json(const nlohmann::json& j, SignUpChallenge& challenge)
{
    uint32_t difficulty{0};
    uint32_t nonce{0};
    uint64_t timestamp{0};

    jsonable::toNumber(j, "difficulty", difficulty);
    jsonable::toNumber(j, "nonce", nonce);
    jsonable::toNumber(j, "timestamp", timestamp);

    challenge.set_difficulty(difficulty);
    challenge.set_nonce(nonce);
    challenge.set_timestamp(timestamp);

}

struct AccountAttributes {
    std::string signalingKey;
    std::string publicKey;
    std::string name;
    std::string nickname;
    bool fetchesMessages{false};
    int registrationId{0};
    std::string deviceName;
    bool voice{false};
    bool video{false};
};

inline void to_json(nlohmann::json& j, const AccountAttributes& attr)
{
    j = nlohmann::json{{"signalingKey", attr.signalingKey},
                       {"publicKey", attr.publicKey},
                       {"name", attr.name},
                       {"nickname", attr.nickname},
                       {"fetchesMessages", attr.fetchesMessages},
                       {"registrationId", attr.registrationId},
                       {"deviceName", attr.deviceName},
                       {"voice", attr.voice},
                       {"video", attr.video}};
}

inline void from_json(const nlohmann::json& j, AccountAttributes& attr)
{
    jsonable::toString(j, "signalingKey", attr.signalingKey);

    if (j.find("pubKey") != j.end()) {
        jsonable::toString(j, "pubKey", attr.publicKey, jsonable::NOT_EMPTY);
    } else if (j.find("publicKey") != j.end()) {
        jsonable::toString(j, "publicKey", attr.publicKey, jsonable::NOT_EMPTY);
    }

    jsonable::toString(j, "name", attr.name, jsonable::OPTIONAL);
    jsonable::toString(j, "nickname", attr.nickname, jsonable::OPTIONAL);
    jsonable::toString(j, "deviceName", attr.deviceName, jsonable::OPTIONAL);

    jsonable::toBoolean(j, "fetchesMessages", attr.fetchesMessages);
    jsonable::toNumber(j, "registrationId", attr.registrationId);
    jsonable::toBoolean(j, "voice", attr.voice);
    jsonable::toBoolean(j, "video", attr.video);
}

struct AccountAttributesSigned {
    std::string sign;
    uint32_t nonce{0};
    AccountAttributes attributes;
};

inline void to_json(nlohmann::json& j, const AccountAttributesSigned& attrSigned)
{
    j = nlohmann::json{{"sign", attrSigned.sign},
                       {"nonce", attrSigned.nonce},
                       {"account", attrSigned.attributes}};
}

inline void from_json(const nlohmann::json& j, AccountAttributesSigned& attrSigned)
{
    jsonable::toString(j, "sign", attrSigned.sign, jsonable::NOT_EMPTY);
    jsonable::toNumber(j, "nonce", attrSigned.nonce, jsonable::OPTIONAL);
    jsonable::toGeneric(j, "account", attrSigned.attributes);
}

struct ApnRegistration {
    std::string apnRegistrationId;
    std::string voipRegistrationId;
    std::string type;
};

inline void to_json(nlohmann::json& j, const ApnRegistration& registration)
{
    j = nlohmann::json{{"apnRegistrationId", registration.apnRegistrationId},
                       {"voipRegistrationId", registration.voipRegistrationId},
                       {"type", registration.type}};
}

inline void from_json(const nlohmann::json& j, ApnRegistration& registration)
{
    jsonable::toString(j, "apnRegistrationId", registration.apnRegistrationId);
    jsonable::toString(j, "voipRegistrationId", registration.voipRegistrationId, jsonable::OPTIONAL);
    jsonable::toString(j, "type", registration.type, jsonable::OPTIONAL);
}

struct GcmRegistration {
    std::string gcmRegistrationId;
    std::string umengRegistrationId;
    bool webSocketChannel{false};
};

inline void to_json(nlohmann::json& j, const GcmRegistration& registration)
{
    j = nlohmann::json{{"gcmRegistrationId", registration.gcmRegistrationId},
                       {"umengRegistrationId", registration.umengRegistrationId},
                       {"webSocketChannel", registration.webSocketChannel}};
}

inline void from_json(const nlohmann::json& j, GcmRegistration& registration)
{
    jsonable::toString(j, "gcmRegistrationId", registration.gcmRegistrationId, jsonable::OPTIONAL);
    jsonable::toString(j, "umengRegistrationId", registration.umengRegistrationId, jsonable::OPTIONAL);
    jsonable::toBoolean(j, "webSocketChannel", registration.webSocketChannel, jsonable::OPTIONAL);
}

// wsh add encrypt number
struct VerificationEncryptNumber {
    std::string phoneNumber;
    std::string verificationCode;
    std::string encryptNumber;
    std::string numberPubkey;
};

inline void to_json(nlohmann::json& j, const VerificationEncryptNumber& verfNumber)
{
    j = nlohmann::json{{"phoneNumber", verfNumber.phoneNumber},
                       {"verificationCode", verfNumber.verificationCode},
                       {"encryptNumber", verfNumber.encryptNumber},
                       {"numberPubkey", verfNumber.numberPubkey}};
}

inline void from_json(const nlohmann::json& j, VerificationEncryptNumber& verfNumber)
{
    jsonable::toString(j, "phoneNumber", verfNumber.phoneNumber, jsonable::OPTIONAL);
    jsonable::toString(j, "verificationCode", verfNumber.verificationCode, jsonable::OPTIONAL);
    jsonable::toString(j, "encryptNumber", verfNumber.encryptNumber, jsonable::OPTIONAL);
    jsonable::toString(j, "numberPubkey", verfNumber.numberPubkey, jsonable::OPTIONAL);
}

struct FeaturesContent {
    std::string features;
};

inline void to_json(nlohmann::json&, const FeaturesContent)
{

}

inline void from_json(const nlohmann::json& j, FeaturesContent& features)
{
    jsonable::toString(j, "features", features.features);
}

}
