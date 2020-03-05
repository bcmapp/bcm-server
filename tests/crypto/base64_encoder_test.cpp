#include "../test_common.h"

#include "crypto/base64.h"
#include <string>

using namespace bcm;

TEST_CASE("Base64Encoder")
{
    const std::string target = "i����efe";
    const std::string encodedTarget = "ae+/ve+/ve+/ve+/vWVmZQ==";
    const std::string encodedTargetUrlsafe = "ae-_ve-_ve-_ve-_vWVmZQ";

    auto encoded = Base64::encode(target);
    auto encodedUrlSafe = Base64::encode(target, true);

    REQUIRE(Base64::decode(Base64::encode(std::string(10, '\0'))) == std::string(10, '\0'));
    REQUIRE(Base64::encode(target) == encodedTarget);
    REQUIRE(Base64::encode(target, true) == encodedTargetUrlsafe);

    REQUIRE(Base64::decode(encodedTarget) == target);
    REQUIRE(Base64::decode("ae+/ve+/ve+/ve+/v\r\nWVmZQ==") == target);
    REQUIRE(Base64::decode(encodedTarget, true) == target);
    REQUIRE(Base64::decode(encodedTargetUrlsafe, true) == target);

    REQUIRE(Base64::check(Base64::encode(std::string(10, '\0'))));
    REQUIRE(Base64::check(encodedTarget));
    REQUIRE(Base64::check(encodedTarget, true));
    REQUIRE(Base64::check(encodedTargetUrlsafe, true));
    REQUIRE_FALSE(Base64::check(encodedTarget + "&"));
    REQUIRE_FALSE(Base64::check(encodedTargetUrlsafe));
    REQUIRE_FALSE(Base64::check(target, true));
    REQUIRE_FALSE(Base64::check(target));
}
