#include "turntoken_generator.h"
#include <utils/time.h>
#include <crypto/hmac.h>
#include <crypto/base64.h>
#include <crypto/random.h>

namespace bcm {

TurnTokenGenerator::TurnTokenGenerator(const bcm::TurnConfig& config)
    : m_config(config)
{
}

TurnToken TurnTokenGenerator::generate()
{
    // implement https://tools.ietf.org/html/draft-uberti-behave-turn-rest-00

    // A value of one day (86400 seconds) is recommended.
    static constexpr int64_t kValidTime = 24 * 60 * 60;

    TurnToken token;
    token.username = std::to_string(nowInSec() + kValidTime) + ":" + std::to_string(SecureRandom<uint32_t>::next());
    token.password = Base64::encode(Hmac::digest(Hmac::Algo::SHA1, m_config.secret, token.username));
    token.urls = m_config.uris;

    return token;
}

}