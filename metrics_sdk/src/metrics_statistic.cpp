
#include "metrics_statistic.h"
#include "../include/metrcis_common.h"


namespace bcm {
namespace metrics {

MetricsStatistic::MetricsStatistic()
    : m_currentTimestampInMs(MetricsCommon::nowInMilli())
    , m_mixMetricsMap()
    , m_counterMetricsMap()
    , m_directOutputMap()
{
}


} //namespace
}