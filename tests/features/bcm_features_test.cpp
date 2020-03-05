#include "../test_common.h"

#include "features/bcm_features.h"
#include "proto/features/features.pb.h"
#include "crypto/hex_encoder.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>

using namespace bcm;

TEST_CASE("BcmFeaturesTest")
{
    BcmFeatures features1(64);
    REQUIRE(HexEncoder::encode(features1.getFeatures()) == "0000000000000000");
    REQUIRE(features1.hasFeature(FEATURE_FEATURES) == false);
    REQUIRE(features1.hasFeature(FEATURE_BIDIRECTIONAL_CONTACT) == false);

    features1.addFeature(FEATURE_FEATURES);
    REQUIRE(HexEncoder::encode(features1.getFeatures()) == "0100000000000000");
    REQUIRE(features1.hasFeature(FEATURE_FEATURES) == true);
    REQUIRE(features1.hasFeature(FEATURE_BIDIRECTIONAL_CONTACT) == false);

    features1.addFeature(FEATURE_BIDIRECTIONAL_CONTACT);
    REQUIRE(HexEncoder::encode(features1.getFeatures()) == "0300000000000000");
    REQUIRE(features1.hasFeature(FEATURE_FEATURES) == true);
    REQUIRE(features1.hasFeature(FEATURE_BIDIRECTIONAL_CONTACT) == true);

    features1.removeFeature(FEATURE_FEATURES);
    REQUIRE(HexEncoder::encode(features1.getFeatures()) == "0200000000000000");
    REQUIRE(features1.hasFeature(FEATURE_FEATURES) == false);
    REQUIRE(features1.hasFeature(FEATURE_BIDIRECTIONAL_CONTACT) == true);

    BcmFeatures features2(HexEncoder::decode(std::string("ff0000000000000000")));
    REQUIRE(features2.hasFeature(FEATURE_FEATURES) == true);
    REQUIRE(features2.hasFeature(FEATURE_BIDIRECTIONAL_CONTACT) == true);
    REQUIRE(HexEncoder::encode(features2.getFeatures()) == "ff0000000000000000");
    nlohmann::json j;
    j.push_back("FEATURE_FEATURES");
    j.push_back("FEATURE_BIDIRECTIONAL_CONTACT");
    std::vector<std::string> s = j;
    REQUIRE(features2.getFeatureNamesJson() == j.dump());
    REQUIRE(features2.getFeatureNames() == s);

    BcmFeatures features3(features1);
    REQUIRE(HexEncoder::encode(features3.getFeatures()) == "0200000000000000");
    REQUIRE(features3.hasFeature(FEATURE_FEATURES) == false);
    REQUIRE(features3.hasFeature(FEATURE_BIDIRECTIONAL_CONTACT) == true);

    BcmFeatures features4(std::move(features2));
    REQUIRE(features4.hasFeature(FEATURE_FEATURES) == true);
    REQUIRE(features4.hasFeature(FEATURE_BIDIRECTIONAL_CONTACT) == true);
    REQUIRE(HexEncoder::encode(features4.getFeatures()) == "ff0000000000000000");

    BcmFeatures features5(0);
    features5 = features3;
    REQUIRE(HexEncoder::encode(features5.getFeatures()) == "0200000000000000");
    REQUIRE(features5.hasFeature(FEATURE_FEATURES) == false);
    REQUIRE(features5.hasFeature(FEATURE_BIDIRECTIONAL_CONTACT) == true);

    BcmFeatures features6(0);
    features6 = std::move(features4);
    REQUIRE(features6.hasFeature(FEATURE_FEATURES) == true);
    REQUIRE(features6.hasFeature(FEATURE_BIDIRECTIONAL_CONTACT) == true);
    REQUIRE(HexEncoder::encode(features6.getFeatures()) == "ff0000000000000000");
}
