#include "../test_common.h"

#include "features/base_features.h"
#include "crypto/hex_encoder.h"
#include <vector>
#include <string>

using namespace bcm;

TEST_CASE("BaseFeaturesTest")
{
    BaseFeatures features1(64);
    REQUIRE(HexEncoder::encode(features1.getFeatures()) == "0000000000000000");
    REQUIRE(features1.hasFeature(0) == false);
    REQUIRE(features1.hasFeature(63) == false);

    features1.addFeature(0);
    REQUIRE(HexEncoder::encode(features1.getFeatures()) == "0100000000000000");
    REQUIRE(features1.hasFeature(0) == true);
    REQUIRE(features1.hasFeature(63) == false);

    features1.addFeature(63);
    REQUIRE(HexEncoder::encode(features1.getFeatures()) == "0100000000000080");
    REQUIRE(features1.hasFeature(0) == true);
    REQUIRE(features1.hasFeature(1) == false);
    REQUIRE(features1.hasFeature(63) == true);

    features1.removeFeature(63);
    REQUIRE(HexEncoder::encode(features1.getFeatures()) == "0100000000000000");
    REQUIRE(features1.hasFeature(0) == true);
    REQUIRE(features1.hasFeature(63) == false);

    BaseFeatures features2(HexEncoder::decode(std::string("ff0000000000000000")));
    REQUIRE(features2.hasFeature(0) == true);
    REQUIRE(features2.hasFeature(7) == true);
    REQUIRE(features2.hasFeature(8) == false);
    REQUIRE(HexEncoder::encode(features2.getFeatures()) == "ff0000000000000000");
    features2.removeFeature(7);
    REQUIRE(features2.hasFeature(0) == true);
    REQUIRE(features2.hasFeature(7) == false);
    REQUIRE(features2.hasFeature(8) == false);
    REQUIRE(HexEncoder::encode(features2.getFeatures()) == "7f0000000000000000");

    BaseFeatures features3(features1);
    REQUIRE(HexEncoder::encode(features3.getFeatures()) == "0100000000000000");
    REQUIRE(features3.hasFeature(0) == true);
    REQUIRE(features3.hasFeature(63) == false);

    BaseFeatures features4(std::move(features2));
    REQUIRE(features4.hasFeature(0) == true);
    REQUIRE(features4.hasFeature(7) == false);
    REQUIRE(features4.hasFeature(8) == false);
    REQUIRE(HexEncoder::encode(features4.getFeatures()) == "7f0000000000000000");

    BaseFeatures features5(0);
    features5 = features3;
    REQUIRE(HexEncoder::encode(features5.getFeatures()) == "0100000000000000");
    REQUIRE(features5.hasFeature(0) == true);
    REQUIRE(features5.hasFeature(63) == false);

    BaseFeatures features6(0);
    features6 = std::move(features4);
    REQUIRE(features6.hasFeature(0) == true);
    REQUIRE(features6.hasFeature(7) == false);
    REQUIRE(features6.hasFeature(8) == false);
    REQUIRE(HexEncoder::encode(features6.getFeatures()) == "7f0000000000000000");
}
