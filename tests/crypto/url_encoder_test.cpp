#include "../test_common.h"

#include <crypto/url_encoder.h>

using namespace bcm;

TEST_CASE("UrlEncoder")
{
    REQUIRE(UrlEncoder::encode("-_~.+") == "-_~.%2B");
    REQUIRE(UrlEncoder::decode("-_~.%2B") == "-_~.+");
    REQUIRE(UrlEncoder::decode("-_~.%2b") == "-_~.+");
}