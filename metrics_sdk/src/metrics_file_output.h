#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include "throttle_control.h"

namespace bcm {
namespace metrics {

class MetricsFileOutput
{
public:
    MetricsFileOutput(std::string dir,
                      uint64_t maxsizeInBytes,
                      uint32_t fileCount,
                      std::string clientId,
                      int64_t writeThreshold);
    ~MetricsFileOutput() = default;

public:
    /**
     * ATTENTION: the MetricsFileOutput is not thread safe currently
     */

    void out(const std::vector<std::string>& outs);

    void flush();

private:
    void rollToNewFile();

    void scanMetricsFiles();

    // call by new thread
    void deleteFileIfExceedMaxCount();

private:
    // configuration for logfile
    std::string m_dir;
    uint64_t m_maxsizeInBytes;
    uint32_t m_fileCount;
    ThrottleControl m_throttleControl;

private:
    std::ofstream m_currentWritingStream;
    uint64_t m_currentWritingSize;
    std::vector<std::string> m_metricsFiles;

public:
    std::string m_metricsPrefix;

};


} //namespace
}