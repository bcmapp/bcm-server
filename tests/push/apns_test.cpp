#include "../test_common.h"
#include "push/apns_client.h"

#include <thread>
#include <chrono>

static const std::string kBundleId = "org.ame.enterprise.im";
static const std::string kCertFile = "../../resource/cert/apns/bcm_enterprise.cert";
static const std::string kPrivKeyFile = "../../resource/cert/apns/bcm_enterprise.key";
static const std::string kType = "bcm_enterprise";

TEST_CASE("ApnsClient_start")
{
    bcm::push::apns::Client cli;
    cli.bundleId(kBundleId).development().certificateFile(kCertFile)
        .privateKeyFile(kPrivKeyFile);

    boost::system::error_code ec;
    cli.start(ec);
    REQUIRE(!ec);
    
    cli.restart(ec);
    REQUIRE(!ec);
}