#pragma once

#include <vector>
#include <map>
#include <redis/hiredis_client.h>

namespace bcm {

typedef std::unordered_map<std::string, std::vector<std::string>> ContactMap;

class ContactTokenManager {
public:
    ContactTokenManager() = default;
    ~ContactTokenManager() = default;

    bool add(const std::string& number, const std::string& uid);
    bool remove(const std::string& number, const std::string& uid);
    bool removeByHash(const std::string& hashNumber, const std::string& uid);
    bool getByToken(const std::string& encodedToken, std::vector<std::string>& uids);
    bool getByTokens(const std::vector<std::string>& encodedTokens, ContactMap& uidsMap);

private:
    std::string decodeToken(const std::string& encoded);
    std::string getKeyByToken(const std::string& token);
    std::string getKeyByNumber(const std::string& number);

private:
    RedisClientSync* m_redisClient{RedisClientSync::Instance()};
};

}

