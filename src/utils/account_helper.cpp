#include "account_helper.h"
#include <bitcoincrypto/Base58Check.hpp>
#include <bitcoincrypto/Sha256.hpp>
#include <bitcoincrypto/Ripemd160.hpp>
#include <boost/algorithm/string.hpp>
#include <signal/curve.h>
#include <signal/signal_protocol.h>
#include <inttypes.h>
#include <regex>
#include <utils/log.h>
#include <utils/time.h>
#include <crypto/base64.h>

namespace bcm {

using namespace boost;

static constexpr uint8_t kUidVersion = 0;

std::string AccountHelper::publicKeyToUid(const std::string& publicKey, bool bWithDjbType)
{
    std::string decoded = Base64::decode(publicKey);
    if (bWithDjbType && !decoded.empty()) {
        decoded = decoded.substr(1);
    }

    Sha256Hash sha256 = Sha256::getHash(reinterpret_cast<const uint8_t*>(decoded.data()), decoded.size());
    uint8_t hash160[Ripemd160::HASH_LEN];
    Ripemd160::getHash(sha256.value, (size_t) Sha256Hash::HASH_LEN, hash160);

    char uid[36];
    Base58Check::pubkeyHashToBase58Check(hash160, kUidVersion, uid);
    return std::string(uid);
}

bool AccountHelper::checkUid(const std::string& their, const std::string& publicKey, bool bWithDjbType)
{
    std::string our = publicKeyToUid(publicKey, bWithDjbType);
    return their == our;
}

bool AccountHelper::validUid(const std::string& uid)
{
    uint8_t hash160[Ripemd160::HASH_LEN];
    uint8_t version{1};
    Base58Check::pubkeyHashFromBase58Check(uid.data(), hash160, &version);
    return version == kUidVersion;
}

bool AccountHelper::verifySignature(const std::string& key, const std::string& message, const std::string& sign)
{
    int ret = 0;
    std::string decodedKey = Base64::decode(key);
    ec_public_key* publicKey = nullptr;
    ret = curve_decode_point(&publicKey, reinterpret_cast<const uint8_t*>(decodedKey.data()),
                             decodedKey.size(), nullptr);
    if (ret < 0) {
        LOGW << "decode public key point failed, error: " << ret;
        return false;
    }

    std::string decodedSign = Base64::decode(sign);
    ret = curve_verify_signature(publicKey,
                                 reinterpret_cast<const uint8_t*>(message.data()), message.size(),
                                 reinterpret_cast<const uint8_t*>(decodedSign.data()), decodedSign.size());
    if (ret < 0) {
        LOGW << "decode public key point failed, error: " << ret;
        return false;
    }

    return ret > 0;
}

Sha256Hash AccountHelper::getChallengeHash(const std::string& uid, uint32_t difficulty,
                                           uint32_t serverNonce, uint32_t clientNonce)
{
    static constexpr uint8_t prefix[] = {'B', 'C', 'M'};
    size_t dataSize = sizeof(prefix) + uid.size() + 3 * sizeof(uint32_t);
    auto data = std::make_unique<uint8_t[]>(dataSize);
    auto* pos = data.get();

    memcpy(pos, prefix, sizeof(prefix));
    pos += sizeof(prefix);

    memcpy(pos, reinterpret_cast<const void*>(uid.data()), uid.size());
    pos += uid.size();

    uint32_t swaped = htobe32(serverNonce);
    memcpy(pos, reinterpret_cast<const void*>(&swaped), sizeof(uint32_t));
    pos += sizeof(uint32_t);

    swaped = htobe32(difficulty);
    memcpy(pos, reinterpret_cast<const void*>(&swaped), sizeof(uint32_t));
    pos += sizeof(uint32_t);

    swaped = htobe32(clientNonce);
    memcpy(pos, reinterpret_cast<const void*>(&swaped), sizeof(uint32_t));

    return Sha256::getDoubleHash(data.get(), dataSize);

}

bool AccountHelper::verifyChallenge(const std::string& uid, uint32_t difficulty,
                                    uint32_t serverNonce, uint32_t clientNonce)
{
    if (difficulty < 1) {
        return true;
    }

    if (difficulty > 32) {
        return false;
    }

    Sha256Hash hash = getChallengeHash(uid, difficulty, serverNonce, clientNonce);

    uint32_t comparing;
    memcpy(reinterpret_cast<void*>(&comparing), reinterpret_cast<const void*>(hash.value), sizeof(uint32_t));
    comparing = be32toh(comparing);

    uint32_t target = static_cast<uint32_t>(pow(2, 32 - difficulty));

    return comparing < target;
}

boost::optional<ClientVersion> AccountHelper::parseClientVersion(const std::string& header)
{
    // header examples:
    // - BCM Android/9.0.0 Model/Vivo_R1S Version/1.0.0 Build/100
    // - BCM iOS/11.0.0 Model/iPhone_A1863 Version/1.0.0 Build/100

    std::vector<std::string> headerParts;
    boost::split(headerParts, header, boost::is_any_of(" "));

    if (headerParts.empty() || headerParts[0] != "BCM") {
        return boost::none;
    }
    headerParts.erase(headerParts.begin());

    ClientVersion clientVersion;
    clientVersion.set_ostype(ClientVersion::OSTYPE_UNKNOWN);
    clientVersion.set_bcmbuildcode(0);

    for (const auto& part : headerParts) {
        std::vector<std::string> partParts;
        boost::split(partParts, part, boost::is_any_of("/"));
        if (partParts.size() < 2) {
            continue;
        }

        if (boost::iequals(partParts[0], "Android")) {
            clientVersion.set_ostype(ClientVersion::OSTYPE_ANDROID);
            clientVersion.set_osversion(partParts[1]);
        } else if (boost::iequals(partParts[0], "iOS")) {
            clientVersion.set_ostype(ClientVersion::OSTYPE_IOS);
            clientVersion.set_osversion(partParts[1]);
        } else if (boost::iequals(partParts[0], "Model")) {
            clientVersion.set_phonemodel(partParts[1]);
        } else if (boost::iequals(partParts[0], "Version")) {
            clientVersion.set_bcmversion(partParts[1]);
        } else if (boost::iequals(partParts[0], "Build")) {
            uint64_t buildCode = 0;
            try {
                buildCode = std::stoull(partParts[1]);
            } catch (const std::exception&) {
                LOGD << "build code is not an integer: " << partParts[1];
            }
            clientVersion.set_bcmbuildcode(buildCode);
        } else if (boost::iequals(partParts[0], "Area")) {
            uint32_t areaCode = 0;
            try {
                areaCode = std::stoul(partParts[1]);
            } catch (const std::exception&) {
                LOGD << "area code is not an integer: " << partParts[1];
            }
            clientVersion.set_areacode(areaCode);
        } else if (boost::iequals(partParts[0], "Lang")) {
            clientVersion.set_langcode(partParts[1]);
        }
    }

    return boost::optional<ClientVersion>(std::move(clientVersion));
}

}
