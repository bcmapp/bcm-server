
#include "test_common.h"
#include "../src/throttle_control.h"
#include "../include/metrcis_common.h"
#include <thread>
#include <iostream>

using namespace bcm::metrics;

TEST_CASE("testReadThreshold")
//void testReadThreshold()
{
    ThrottleControl throttleControl(1024 * 1024, 0);
    throttleControl.start();

    int64_t now = MetricsCommon::nowInMilli();
    for (int i=0; i<3; ++i) {
        throttleControl.checkReadQuota(1024 * 512);
    }
    int64_t after = MetricsCommon::nowInMilli();
    int64_t diff = after - now;
    std::cout << "testReadThreshold using :" << diff << "(ms)" << std::endl;
    REQUIRE(diff > 30000);
}

TEST_CASE("testWriteThreshold")
//void testWriteThreshold()
{
    ThrottleControl throttleControl(0, 1024 * 1024);
    throttleControl.start();

    int64_t now = MetricsCommon::nowInMilli();
    for (int i=0; i<3; ++i) {
        throttleControl.checkWriteQuota(1024 * 512);
    }
    int64_t after = MetricsCommon::nowInMilli();
    int64_t diff = after - now;
    std::cout << "testWriteThreshold using :" << diff << "(ms)" << std::endl;
    REQUIRE(diff > 30000);
}