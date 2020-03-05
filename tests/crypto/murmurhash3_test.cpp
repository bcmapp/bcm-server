#include "../test_common.h"

#include "crypto/murmurhash3.h"
#include <vector>
#include <string>

using namespace bcm;

TEST_CASE("mrumurHash3Test")
{
    const std::string testStr = "Bcm";
    std::vector<uint8_t> target;
    target.assign(testStr.begin(), testStr.end());

    REQUIRE(MurmurHash3::murmurHash3(0, target) == 1378646842);
    REQUIRE(MurmurHash3::murmurHash3(3, target) == 1350417861);

    const std::string testHello = "Hello, world!";
    target.assign(testHello.begin(), testHello.end());
    REQUIRE(MurmurHash3::murmurHash3(0, target) == 3224780355);

    REQUIRE(MurmurHash3::murmurHash3(0, testStr) == 1378646842);
    REQUIRE(MurmurHash3::murmurHash3(3, testStr) == 1350417861);
    REQUIRE(MurmurHash3::murmurHash3(0, testHello) == 3224780355);
    REQUIRE(MurmurHash3::murmurHash3(0, "123456789") == 3036607362);
}
