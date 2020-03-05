#include "../test_common.h"

#include <crypto/any_base.h>

using namespace bcm;

TEST_CASE("AnyBase")
{
    std::string encoded;
    uint64_t decoded;

    REQUIRE(AnyBase::encode(UINT64_MAX, 62, encoded));
    REQUIRE(AnyBase::decode(encoded, 62, decoded));
    REQUIRE(decoded == UINT64_MAX);

    REQUIRE_FALSE(AnyBase::encode(UINT64_MAX, 63, encoded));
    REQUIRE_FALSE(AnyBase::decode(encoded, 63, decoded));
}
