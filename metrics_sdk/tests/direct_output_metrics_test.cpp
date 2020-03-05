#include "metrics_types/direct_output_metrics.h"
#include <vector>
#include <string>
#include <iostream>
#include "test_common.h"

using namespace bcm::metrics;

TEST_CASE("testDirectOutputMetricsData")
//void testDirectOutputMetricsData()
{
    DirectOutputMetrics directOutputMetrics;
    directOutputMetrics.m_metricsName = "metricsName";
    directOutputMetrics.m_currentTimestampInMs = 321L;
    directOutputMetrics.mark("this is a test1");

    std::vector<std::string> outputs;
    directOutputMetrics.getMetricsOutput(outputs);
    REQUIRE(outputs.size() == 1);
    REQUIRE(outputs[0] == "metricsName,321,this is a test1");

    directOutputMetrics.mark("this is a test2");
    directOutputMetrics.mark("this is a test3");
    outputs.clear();
    directOutputMetrics.getMetricsOutput(outputs);
    REQUIRE(outputs.size() == 3);
    REQUIRE(outputs[1] == "metricsName,321,this is a test2");
    REQUIRE(outputs[2] == "metricsName,321,this is a test3");

}