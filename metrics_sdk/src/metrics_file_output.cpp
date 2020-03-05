#include "metrics_file_output.h"
#include <iostream>
#include "metrics_log_utils.h"
#include <time.h>
#include <fstream>
#include <vector>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <iomanip>
#include <algorithm>
#include <string>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include "metrics_define.h"

namespace bcm {
namespace metrics {

MetricsFileOutput::MetricsFileOutput(std::string dir,
                                     uint64_t maxsizeInBytes,
                                     uint32_t fileCount,
                                     std::string clientId,
                                     int64_t writeThreshold)
    : m_throttleControl(10, writeThreshold)
    , m_currentWritingSize(0)
{
    METRICS_ASSERT(clientId.size() == 5, "metrics config: client Id should be 5 character");
    METRICS_ASSERT(dir != "", "metrics config: metrics dir should not empty");
    METRICS_ASSERT(maxsizeInBytes != 0, "metrics config: metricsFileSizeInBytes should not 0");
    METRICS_ASSERT(fileCount != 0, "metrics config: metricsFileCount should not 0");

    m_metricsPrefix = "bcm_metrics_" + clientId + "_";

    // add / to file path
    if (dir.back() != '/') {
        dir = dir + "/";
    }

    if (0 != access(dir.c_str(), R_OK)) {
        METRICS_LOG_INFO("begin create metrics output dir: %s", dir.c_str());
        int ret = mkdir(dir.c_str(), S_IRWXU);
        METRICS_ASSERT(ret == 0, "cannot create metrics output dir: " + dir);
    }

    m_dir = dir;
    m_maxsizeInBytes = maxsizeInBytes;
    m_fileCount = fileCount;
    m_throttleControl.start();

    METRICS_LOG_INFO("init metrics file with dir: %s; maxsize in bytes: %" PRIu64 "; file count: %d",
                     m_dir.c_str(), m_maxsizeInBytes, m_fileCount);

    scanMetricsFiles();
    rollToNewFile();
}

void MetricsFileOutput::out(const std::vector<std::string> &outs)
{
    if (m_currentWritingSize > m_maxsizeInBytes) {
        rollToNewFile();
    }

    if (m_currentWritingStream.is_open()) {
        for (auto& c : outs) {
            auto size = c.size();
            m_throttleControl.checkWriteQuota(size);
            m_currentWritingStream << c << '\n';
            m_currentWritingSize += size;
        }
    } else {
        METRICS_LOG_ERR("output metrics file error, m_currentWritingStream is close");
    }
}

void MetricsFileOutput::flush()
{
    if (m_currentWritingStream.is_open()) {
        m_currentWritingStream.flush();
    } else {
        METRICS_LOG_ERR("flush metrics file error, m_currentWritingStream is close");
    }
}

void MetricsFileOutput::rollToNewFile()
{
    // get current time
    std::time_t now = std::time(0);
    std::tm *tm = std::localtime(&now);
    // build new file name
    std::stringstream file_name_ss;
    file_name_ss << std::put_time(tm, "%Y%m%d_%H%M%S");
    std::string fileName = m_metricsPrefix + file_name_ss.str() + ".log";
    std::string newFileWithPath =  m_dir + fileName;

    // close last log file
    if (m_currentWritingStream.is_open()) {
        m_currentWritingStream.flush();
        m_currentWritingStream.close();
        m_currentWritingStream.clear();
    }

    // create a new file stream to write
    m_currentWritingStream.open(newFileWithPath.c_str(), std::ios::out);
    if (!m_currentWritingStream.is_open()) {
        METRICS_ASSERT(false, "open metrics log file failed, file: " + newFileWithPath);
    }
    m_currentWritingSize = 0;
    m_metricsFiles.emplace_back(fileName);

    METRICS_LOG_INFO("finish roll to new metric file: %s", newFileWithPath.c_str());

    // check and delete max file if need
    deleteFileIfExceedMaxCount();
}

void MetricsFileOutput::scanMetricsFiles()
{
    // list files in log dir
    DIR *dp;
    struct dirent *dirp;
    if((dp  = opendir(m_dir.c_str())) == NULL) {
        METRICS_LOG_ERR("cannot open dir: %s", m_dir.c_str());
        return;
    }
    while ((dirp = readdir(dp)) != NULL) {
        std::string fileName = std::string(dirp->d_name);
        // is log file
        if (fileName.find(m_metricsPrefix) == 0) {
            m_metricsFiles.emplace_back(fileName);
        }
    }
    closedir(dp);

    std::sort(m_metricsFiles.begin(), m_metricsFiles.end());
}

void MetricsFileOutput::deleteFileIfExceedMaxCount()
{
    // check file count
    if (m_metricsFiles.size() <= m_fileCount) {
        METRICS_LOG_TRACE("does not reach the max file count, does not need to delete file");
        return;
    }
    std::sort(m_metricsFiles.begin(), m_metricsFiles.end());

    // do delete
    while (m_metricsFiles.size() > m_fileCount) {
        std::string s = *m_metricsFiles.begin();
        std::string removeFileNameWithPath = m_dir + s;
        m_metricsFiles.erase(m_metricsFiles.begin());
        int ret_code = std::remove(removeFileNameWithPath.c_str());
        if (ret_code == 0) {
            METRICS_LOG_INFO("delete metric file success, name: %s", removeFileNameWithPath.c_str());
        } else {
            METRICS_LOG_ERR("delete metric file fail, name: %s; ret_code: %d", removeFileNameWithPath.c_str(), ret_code);
        }
    }

}

} //namespace
}