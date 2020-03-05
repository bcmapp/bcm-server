#include "../test_common.h"

#include <utils/account_helper.h>
#include <phonenumbers/phonenumberutil.h>
#include <phonenumbers/region_code.h>

using namespace bcm;

TEST_CASE("AccountUtils")
{
    REQUIRE(AccountHelper::publicKeyToUid("oXtnEQ+gwP9rYGSlrygvJ6pgQPYC2H9CW6bx3RTqeEU=", false)
            == "1EGkR3FrBNfoGQCZApWoZP7vVmmhfuKesb");
    REQUIRE(AccountHelper::publicKeyToUid("BaF7ZxEPoMD/a2Bkpa8oLyeqYED2Ath/Qlum8d0U6nhF")
            == "1EGkR3FrBNfoGQCZApWoZP7vVmmhfuKesb");

    REQUIRE(AccountHelper::checkUid("1EGkR3FrBNfoGQCZApWoZP7vVmmhfuKesb", "BaF7ZxEPoMD/a2Bkpa8oLyeqYED2Ath/Qlum8d0U6nhF"));
    REQUIRE(AccountHelper::checkUid("1EGkR3FrBNfoGQCZApWoZP7vVmmhfuKesb", "oXtnEQ+gwP9rYGSlrygvJ6pgQPYC2H9CW6bx3RTqeEU=",
                                   false));

    REQUIRE(AccountHelper::validUid("1EGkR3FrBNfoGQCZApWoZP7vVmmhfuKesb"));

    REQUIRE(AccountHelper::verifySignature("BdIBKuWQtJIil3L/i2hGJKQPirD1b9lZSMWN8AJ8jZAi", "1667593234",
                                           "6HM/C9jGj1pJeaCT56z04RVbSs6qxOFPSN8ckQVHuJwI7k6i9pV/elQ+LnE7"\
                                           "r/kcqBVADfmZM19A8BbaqS0IDg=="));

    REQUIRE(AccountHelper::getChallengeHash("1wNmWdS1v8Q2qPyc9oVyruGaUtMB4pXpk", 16, 1181581746, 16816)
            == Sha256Hash("1f14cf1acfd34f9bd5d4a5ccb7a231713e0f09a1e49c913621bf118a87620452"));

    REQUIRE_FALSE(AccountHelper::verifyChallenge("1wNmWdS1v8Q2qPyc9oVyruGaUtMB4pXpk", 16, 1181581746, 16816));
    REQUIRE(AccountHelper::verifyChallenge("17CK7xV3pKu3y6j2McwA9pLuHBMS4fatM4", 16, 79355200, 395615));

    boost::optional<ClientVersion> clientVersion;

    clientVersion = AccountHelper::parseClientVersion("BCMX Android/9.0.0 Model/Vivo_R1S Version/1.0.0 Build/100");
    REQUIRE(!clientVersion);

    clientVersion = AccountHelper::parseClientVersion("BCM Android/9.0.0 Model/Vivo_R1S Version/1.0.0 Build/100");
    REQUIRE(!!clientVersion);
    REQUIRE(clientVersion->ostype() == ClientVersion::OSTYPE_ANDROID);
    REQUIRE(clientVersion->osversion() == "9.0.0");
    REQUIRE(clientVersion->phonemodel() == "Vivo_R1S");
    REQUIRE(clientVersion->bcmversion() == "1.0.0");
    REQUIRE(clientVersion->bcmbuildcode() == 100);

    clientVersion = AccountHelper::parseClientVersion("BCM iOS/11.0.0 Model/iPhone_A1863 Version/2.0.0 Build/200");
    REQUIRE(!!clientVersion);
    REQUIRE(clientVersion->ostype() == ClientVersion::OSTYPE_IOS);
    REQUIRE(clientVersion->osversion() == "11.0.0");
    REQUIRE(clientVersion->phonemodel() == "iPhone_A1863");
    REQUIRE(clientVersion->bcmversion() == "2.0.0");
    REQUIRE(clientVersion->bcmbuildcode() == 200);

    clientVersion = AccountHelper::parseClientVersion("BCM Build/code");
    REQUIRE(!!clientVersion);
    REQUIRE(clientVersion->ostype() == ClientVersion::OSTYPE_UNKNOWN);
    REQUIRE(clientVersion->bcmbuildcode() == 0);

}

TEST_CASE("SignVerify")
{
    std::string pubKey = "BXdq6V2l/B45bGmpBgmQFM2Ns822BB8MEl3csKCyEXAc";
    std::string signature = "9DZXdRubG8UoYmBAIxBB9Oz/6xrB5bpqaK5dgk1XlUnClOHpQo63/rHBDyqBIFLRIBv2LjrwfHA/Aoo5Xe8Hjg==";
    std::string toVerify = "1KjWwsKeNVRCWb7177FxMr6pqTqxmUmSR14ZrnDg8khSK4Gw1BKPWh3hCq3AQHPTmnC1558407688162Optional(\"{\\n  \\\"handleBackground\\\" : \\\"true\\\",\\n  \\\"nameKey\\\" : \\\"KYu1wVQK2O68p0R1JQ4d34\\\\/ZId4sxh5u8UalGLZnJUX2oe5QO1IV4Fur4schbA1E\\\",\\n  \\\"avatarKey\\\" : \\\"\\\",\\n  \\\"version\\\" : 1\\n}\")/v2/contacts/friends/request";

    bool isOk = AccountHelper::verifySignature(pubKey, toVerify, signature);
    REQUIRE(isOk);
}