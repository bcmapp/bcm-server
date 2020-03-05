#include "contact_token_manager.h"
#include "utils/number_utils.h"
#include <utils/account_helper.h>
#include <crypto/hex_encoder.h>
#include <crypto/base64.h>
#include <boost/algorithm/string.hpp>

namespace bcm {

static constexpr char kKeyPrefix[] = "contact";

bool ContactTokenManager::add(const std::string& number, const std::string& uid)
{
    return m_redisClient->sadd(getKeyByNumber(number), uid);
}

bool ContactTokenManager::remove(const std::string& number, const std::string& uid)
{
    return m_redisClient->srem(getKeyByNumber(number), uid);
}

bool ContactTokenManager::removeByHash(const std::string& hashNumber, const std::string& uid)
{
    std::string hashToken = Base64::decode(hashNumber);
    return m_redisClient->srem(getKeyByToken(hashToken), uid);
}


bool ContactTokenManager::getByToken(const std::string& encodedToken, std::vector<std::string>& uids)
{
    return m_redisClient->smembers(getKeyByToken(decodeToken(encodedToken)), uids);
}

bool ContactTokenManager::getByTokens(const std::vector<std::string>& encodedTokens, ContactMap& uidsMap)
{
    std::set<std::string> decodedTokens;
    std::map<std::string, std::string> tokensMap;
    for (auto& encodedToken : encodedTokens) {
        auto key = getKeyByToken(decodeToken(encodedToken));
        decodedTokens.insert(key);
        tokensMap[key] = encodedToken;
    }

    ContactMap innerMap;
    auto ret = m_redisClient->smembersBatch(decodedTokens, innerMap);
    if (!ret) {
        return false;
    }

    for (auto& item : innerMap) {
        uidsMap[tokensMap[item.first]] = item.second;
    }

    return true;
}

std::string ContactTokenManager::decodeToken(const std::string& encoded)
{
    return Base64::decode(encoded, true);
}

std::string ContactTokenManager::getKeyByNumber(const std::string& number)
{
    return getKeyByToken(NumberUtils::getNumberToken(number));
}

std::string ContactTokenManager::getKeyByToken(const std::string& token)
{
    return kKeyPrefix + token;
}

}