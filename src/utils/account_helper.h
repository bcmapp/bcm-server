#pragma once

#include <string>
#include <boost/optional.hpp>
#include <bitcoincrypto/Sha256Hash.hpp>
#include <proto/dao/client_version.pb.h>


namespace bcm {

class AccountHelper {
public:
    static std::string publicKeyToUid(const std::string& publicKey, bool bWithDjbType = true);

    static bool checkUid(const std::string& their, const std::string& publicKey, bool bWithDjbType = true);

    static bool validUid(const std::string& uid);

    static bool verifySignature(const std::string& key, const std::string& message, const std::string& sign);

    static Sha256Hash getChallengeHash(const std::string& uid, uint32_t difficulty,
                                       uint32_t serverNonce, uint32_t clientNonce);

    static bool verifyChallenge(const std::string& uid, uint32_t difficulty,
                                uint32_t serverNonce, uint32_t clientNonce);

    static boost::optional<ClientVersion> parseClientVersion(const std::string& header);
};
}
