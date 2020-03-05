#include "metrics_types/mix_metrics.h"
#include "test_common.h"
#include <vector>
#include <string>
#include <iostream>

using namespace bcm::metrics;

TEST_CASE("testMixMetricsData")
//void testMixMetricsData()
{
    MixMetrics mixMetrics;
    mixMetrics.m_topic = "topic";
    mixMetrics.m_serviceName = "serviceName";
    mixMetrics.m_appName = "appName";
    mixMetrics.m_appVersion = "appVersion";
    mixMetrics.m_currentTimestampInMs = 555555;

    // first metrics data
    mixMetrics.markDuration(100);
    mixMetrics.m_retCodeMap.emplace("200", (uint64_t)1);

    // insert metrics data
    mixMetrics.markDuration(200);
    mixMetrics.m_retCodeMap.at("200") = 2;

    mixMetrics.markDuration(300);
    mixMetrics.m_retCodeMap.emplace("201", (uint64_t)1);

    mixMetrics.markDuration(400);
    mixMetrics.m_retCodeMap.at("200") = 3;

    mixMetrics.markDuration(500);
    mixMetrics.m_retCodeMap.at("201") = 2;

    mixMetrics.markDuration(600);
    mixMetrics.m_retCodeMap.emplace("202", (uint64_t)1);

    std::vector<std::string> result;
    mixMetrics.getMetricsOutput(result);
    REQUIRE(result.size() == 3);

    std::string expectResult = "mix,555555,serviceName,topic,appVersion,3,200,350";
    std::cout << result[0] << std::endl;
    REQUIRE(result[0] == expectResult);

    expectResult = "mix,555555,serviceName,topic,appVersion,2,201,350";
    std::cout << result[1] << std::endl;
    REQUIRE(result[1] == expectResult);

    expectResult = "mix,555555,serviceName,topic,appVersion,1,202,350";
    std::cout << result[2] << std::endl;
    REQUIRE(result[2] == expectResult);

}