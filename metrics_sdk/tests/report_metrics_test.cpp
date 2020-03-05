#include "test_common.h"
#include "report_metrics.h"

using namespace bcm::metrics;

TEST_CASE("testReportMetricsCopy")
//void testReportMetricsCopy()
{
    ReportMetrics src;
    src.m_int64t1 = 1;
    src.m_int64t2 = 2;
    src.m_string1 = "200";
    src.m_string2 = "topic1";
    src.m_string3 = "serviceName1";
    src.m_type = ReportMetricsType::UNKNOWN;

    // test operation =
    ReportMetrics copy1 = src;
    src.m_string1 = "201";
    REQUIRE(copy1.m_int64t1 == 1);
    REQUIRE(copy1.m_int64t2 == 2);
    REQUIRE(copy1.m_string1 == "200");
    REQUIRE(copy1.m_string2 == "topic1");
    REQUIRE(copy1.m_string3 == "serviceName1");
    REQUIRE(copy1.m_type == ReportMetricsType::UNKNOWN);

    // test copy constructor
    src.m_string1 = "200";
    ReportMetrics copy2(src);
    src.m_string1 = "201";
    REQUIRE(copy2.m_int64t1 == 1);
    REQUIRE(copy2.m_int64t2 == 2);
    REQUIRE(copy2.m_string1 == "200");
    REQUIRE(copy2.m_string2 == "topic1");
    REQUIRE(copy2.m_string3 == "serviceName1");
    REQUIRE(copy2.m_type == ReportMetricsType::UNKNOWN);

    // test std move
    src.m_string1 = "200";
    ReportMetrics copy3(std::move(src));
    REQUIRE(copy3.m_int64t1 == 1);
    REQUIRE(copy3.m_int64t2 == 2);
    REQUIRE(copy3.m_string1 == "200");
    REQUIRE(copy3.m_string2 == "topic1");
    REQUIRE(copy3.m_string3 == "serviceName1");
    REQUIRE(copy3.m_type == ReportMetricsType::UNKNOWN);

}