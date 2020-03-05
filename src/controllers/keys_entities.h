#pragma once

#include <utils/jsonable.h>
#include <proto/dao/pre_key.pb.h>

namespace bcm {

struct PreKeyCount {
    uint32_t count;
};

inline void to_json(nlohmann::json& j, const PreKeyCount& keyCount) {
    j = nlohmann::json{{"count", keyCount.count}};
}

inline void from_json(const nlohmann::json& j, PreKeyCount& keyCount) {
    jsonable::toNumber(j, "count", keyCount.count);
}


//message PreKey {
//    uint64 keyId = 1;
//    string publicKey = 2;
//}
inline void to_json(nlohmann::json& j, const PreKey& preKey) {
    j = nlohmann::json{{"keyId", preKey.keyid()},
                       {"publicKey", preKey.publickey()}};
}

inline void from_json(const nlohmann::json& j, PreKey& preKey) {
    uint64_t keyId{0};
    std::string publicKey;

    jsonable::toNumber(j, "keyId", keyId);
    jsonable::toString(j, "publicKey", publicKey);

    preKey.set_keyid(keyId);
    preKey.set_publickey(publicKey);
}


//message SignedPreKey {
//    uint64 keyId = 1;
//    string publicKey = 2;
//    string signature = 3;
//}

inline bool  operator!=(const SignedPreKey& l, const SignedPreKey& r)
{
    return (l.keyid() != r.keyid()) || (l.publickey() != r.publickey()) || (l.signature() != r.signature());
}

inline void to_json(nlohmann::json& j, const SignedPreKey& signedPreKey) {
    j = nlohmann::json{{"keyId", signedPreKey.keyid()},
                       {"publicKey", signedPreKey.publickey()},
                       {"signature", signedPreKey.signature()}};
}

inline void from_json(const nlohmann::json& j, SignedPreKey& signedPreKey) {
    uint64_t keyId{0};
    std::string publicKey;
    std::string signature;

    jsonable::toNumber(j, "keyId", keyId);
    jsonable::toString(j, "publicKey", publicKey);
    jsonable::toString(j, "signature", signature);

    signedPreKey.set_keyid(keyId);
    signedPreKey.set_publickey(publicKey);
    signedPreKey.set_signature(signature);
}


struct PreKeyState {
    std::vector<PreKey> preKeys;
    SignedPreKey signedPreKey;
    std::string identityKey; // deprecated
};

inline void to_json(nlohmann::json& j, const PreKeyState& keyState) {
    j = nlohmann::json{{"preKeys", keyState.preKeys},
                       {"signedPreKey", keyState.signedPreKey},
                       {"identityKey",keyState.identityKey}};
}

inline void from_json(const nlohmann::json& j, PreKeyState& keyState) {
    jsonable::toGeneric(j, "preKeys", keyState.preKeys);
    jsonable::toGeneric(j, "signedPreKey", keyState.signedPreKey);
    jsonable::toString(j, "identityKey", keyState.identityKey);
}


struct PreKeyResponseItem {
    int deviceId;
    int registrationId;
    std::string deviceIdentityKey;
    std::string accountSignature;
    boost::optional<SignedPreKey> signedPreKey;
    boost::optional<PreKey> preKey;
};

inline void to_json(nlohmann::json& j, const PreKeyResponseItem& item) {
    j = nlohmann::json{{"deviceId", item.deviceId},
                       {"registrationId", item.registrationId},
                       {"deviceIdentityKey", item.deviceIdentityKey},
                       {"accountSignature", item.accountSignature},
                       {"signedPreKey", item.signedPreKey},
                       {"preKey", item.preKey}};
}

inline void from_json(const nlohmann::json& j, PreKeyResponseItem& item) {
    jsonable::toNumber(j, "deviceId", item.deviceId);
    jsonable::toNumber(j, "registrationId", item.registrationId);
    jsonable::toString(j, "deviceIdentityKey", item.deviceIdentityKey);
    jsonable::toString(j, "accountSignature", item.accountSignature);
    jsonable::toGeneric(j, "signedPreKey", item.signedPreKey);
    jsonable::toGeneric(j, "preKey", item.preKey);
}

struct PreKeyResponse {
    std::string identityKey;
    std::vector<PreKeyResponseItem> devices;
};

inline void to_json(nlohmann::json& j, const PreKeyResponse& response) {
    j = nlohmann::json{{"identityKey", response.identityKey},
                       {"devices", response.devices}};
}

inline void from_json(const nlohmann::json& j, PreKeyResponse& response) {
    jsonable::toString(j, "identityKey", response.identityKey);
    jsonable::toGeneric(j, "devices", response.devices);
}

}
