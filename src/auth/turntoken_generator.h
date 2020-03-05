#pragma once

#include <config/turn_config.h>
#include <utils/jsonable.h>

namespace bcm {

struct TurnToken {
    std::string username;
    std::string password;
    std::vector<std::string> urls;
};

inline void to_json(nlohmann::json& j, const TurnToken& token)
{
    j = nlohmann::json{{"username", token.username},
                       {"password", token.password},
                       {"urls", token.urls}};
}

inline void from_json(const nlohmann::json& j, TurnToken& token)
{
    jsonable::toString(j, "username", token.username);
    jsonable::toString(j, "password", token.password);
    jsonable::toGeneric(j, "urls", token.urls);
}

class TurnTokenGenerator {
public:
    explicit TurnTokenGenerator(const TurnConfig& config);

    TurnToken generate();

private:
    TurnConfig m_config;
};

}

