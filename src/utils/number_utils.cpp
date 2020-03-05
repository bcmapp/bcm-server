#include "number_utils.h"
#include <phonenumbers/phonenumberutil.h>
#include <phonenumbers/region_code.h>
#include <boost/uuid/detail/sha1.hpp>
#include <crypto/random.h>
#include "../crypto/base64.h"

namespace bcm {

bool NumberUtils::isValidNumber(const std::string& number)
{
    using namespace i18n::phonenumbers;

    PhoneNumber phoneNumber;
    auto utils = PhoneNumberUtil::GetInstance();
    utils->ParseAndKeepRawInput(number, RegionCode::ZZ(), &phoneNumber);
    return utils->IsValidNumber(phoneNumber);
}

bool NumberUtils::isTestNumber(const std::string& number)
{
    try {
        auto numberLong = std::stol(number);
        return (8610000000000l <= numberLong && numberLong < 8611000000000l);
    } catch (std::exception&) {
        return false;
    }
}

std::string NumberUtils::getNumberToken(const std::string& number)
{
    boost::uuids::detail::sha1 sha;
    uint32_t digest[5] = {0};
    sha.process_bytes(number.data(), number.size());
    sha.get_digest(digest);

    int swaped = 0;
    char hash[20] = {0};
    for (int i = 0; i < 5; ++i) {
        swaped = htobe32(digest[i]);
        memcpy(hash + i * sizeof(uint32_t), &swaped, sizeof(uint32_t));
    }

    return std::string(hash, 10);
}

std::string NumberUtils::getBase64NumberToken(const std::string& number)
{
    return Base64::encode(getNumberToken(number));
}

std::string NumberUtils::genVerificationCode(const std::string& number)
{
    if (isTestNumber(number)) {
        return "1234";
    }

    int code;
    code = SecureRandom<int>::next(9999);
    char buf[5] = {0};
    std::snprintf(buf, 5, "%04d", code);
    return std::string(buf, 4);
}

}
