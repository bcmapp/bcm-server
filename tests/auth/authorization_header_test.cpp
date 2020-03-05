#include "../test_common.h"

#include <auth/authorization_header.h>
#include <crypto/base64.h>
#include <proto/dao/device.pb.h>

using namespace bcm;

TEST_CASE("AuthorizationHeader")
{
    std::string header;
    boost::optional<AuthorizationHeader> auth;

    header = "Basic " + Base64::encode("uid:token");
    auth = AuthorizationHeader::parse(header);
    REQUIRE(!!auth);
    REQUIRE(auth->uid() == "uid");
    REQUIRE(auth->token() == "token");
    REQUIRE(auth->deviceId() == Device::MASTER_ID);

    header = "Basic " + Base64::encode("uid.2:token");
    auth = AuthorizationHeader::parse(header);
    REQUIRE(!!auth);
    REQUIRE(auth->uid() == "uid");
    REQUIRE(auth->token() == "token");
    REQUIRE(auth->deviceId() == 2);

    header = Base64::encode("uid.2:token");
    auth = AuthorizationHeader::parse(header);
    REQUIRE(!auth);

    header = "Upgrade " + Base64::encode("uid.2:token");
    auth = AuthorizationHeader::parse(header);
    REQUIRE(!auth);

    header = "Basic xxx";
    auth = AuthorizationHeader::parse(header);
    REQUIRE(!auth);

    header = "Basic " + Base64::encode("uid.2-token");
    auth = AuthorizationHeader::parse(header);
    REQUIRE(!auth);
}
