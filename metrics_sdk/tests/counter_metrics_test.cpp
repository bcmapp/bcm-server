#include "metrics_types/counter_metrics.h"
#include <vector>
#include <string>
#include <iostream>
#include "test_common.h"

using namespace bcm::metrics;

TEST_CASE("testCounterMetricsData")
//void testCounterMetricsData()
{
    CounterMetrics counterMetrics;
    counterMetrics.m_counterName = "testCounterName";
    counterMetrics.m_currentTimestampInMs = 123L;
    counterMetrics.set(1);

    std::vector<std::string> outputs;
    counterMetrics.getMetricsOutput(outputs);
    REQUIRE(outputs.size() == 1);
    REQUIRE(outputs[0] == "testCounterName,123,1");

    counterMetrics.add(10);
    outputs.clear();
    counterMetrics.getMetricsOutput(outputs);
    REQUIRE(outputs.size() == 1);
    REQUIRE(outputs[0] == "testCounterName,123,11");

    counterMetrics.set(100);
    outputs.clear();
    counterMetrics.getMetricsOutput(outputs);
    REQUIRE(outputs.size() == 1);
    REQUIRE(outputs[0] == "testCounterName,123,100");

}