#include "../test_common.h"

#include "metrics/onlineuser_metrics.h"
#include "dispatcher/dispatch_manager.h"
#include "config/dispatcher_config.h"
#include <metrics_client.h>
#include "config/encrypt_sender.h"
#include <vector>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <algorithm>
#include <dirent.h>

using namespace bcm;
using namespace bcm::metrics;

std::string ALL_METRIC_PREFIX = "bcm_metrics_";
std::string TEST_METRIC_PREFIX = "bcm_metrics_00001_";

void testListMetricsFile(std::vector<std::string>& logFilesNames, std::string prefix)
{
    DIR *dp;
    struct dirent *dirp;
    if((dp  = opendir("/tmp/")) == NULL) {
        std::cout << "cannot open dir: " << std::endl;
        return;
    }
    while ((dirp = readdir(dp)) != NULL) {
        std::string fileName = std::string(dirp->d_name);
        // is log file
        if (fileName.find(prefix) == 0) {
            logFilesNames.push_back(fileName);
        }
    }
    closedir(dp);

    std::sort(logFilesNames.begin(), logFilesNames.end());
}

TEST_CASE("testOnlineUserMetrics")
//void testOnlineUserMetrics()
{

    std::string rmStr = "exec rm -rf /tmp/bcm_metrics_*";
    system(rmStr.c_str());

    MetricsConfig config;
    config.appVersion = "1.0";
    config.reportQueueSize = 5000;
    config.metricsDir = "/tmp";
    config.metricsFileSizeInBytes = 1024*10;
    config.metricsFileCount = 5;
    config.reportIntervalInMs = 3000;
    config.clientId = "00001";
    config.writeThresholdInBytes = 1024 * 1024;
    MetricsClient::Init(config);

    DispatcherConfig dispatcherConfig;
    EncryptSenderConfig encryptSenderConfig;

    auto dispatchManager = std::make_shared<DispatchManager>(dispatcherConfig,
                                                             nullptr, nullptr, nullptr,
                                                             encryptSenderConfig);

    DispatchAddress addr1("uid_1", 0);
    DispatchAddress addr2("uid_2", 0);

    auto onlineUserMetricsReporter = std::make_shared<OnlineUserMetrics>(dispatchManager, 1000);
    dispatchManager->replaceDispatcher(addr1, nullptr);
    dispatchManager->replaceDispatcher(addr2, nullptr);
    onlineUserMetricsReporter->start();
    sleep(4);

    std::vector<std::string> logFilesNames;
    std::string line;
    testListMetricsFile(logFilesNames, TEST_METRIC_PREFIX);

    std::ifstream myfile("/tmp/" + logFilesNames[0]);
    auto opened = myfile.is_open();
    REQUIRE(opened);
    if (opened) {
        line = "";
        getline(myfile, line);
        std::cout << line << std::endl;
        REQUIRE(line.substr(0, 12) == "o_onlineuser");
        REQUIRE(line.substr(27) == "2");
    }

    DispatchAddress addr3("uid_3", 0);
    dispatchManager->replaceDispatcher(addr3, nullptr);

    sleep(3);
    line = "";
    getline(myfile, line);
    std::cout << line << std::endl;
    REQUIRE(line.substr(0, 12) == "o_onlineuser");
    REQUIRE(line.substr(27) == "3");
    myfile.close();

    return;

}


