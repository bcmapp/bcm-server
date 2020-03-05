#include "metrics_file_output.h"
#include "metrics_test_common.h"
#include "test_common.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>

using namespace bcm::metrics;

TEST_CASE("testMetricsFileOutputContent")
//void testMetricsFileOutputContent()
{
    std::string rmStr = "exec rm -rf /tmp";
    system(rmStr.c_str());
    MetricsFileOutput metricsFileOutput("/tmp", 10 * 1024, 5, "00001", 10*1024*1024);

    std::string str = "1234567890"; // 10 bytes
    std::vector<std::string> metricsVector;
    metricsVector.push_back(str);

    metricsFileOutput.out(metricsVector);
    metricsFileOutput.flush();

    std::vector<std::string> logFilesNames;
    std::string line;
    testListMetricsFile(logFilesNames, TEST_METRIC_PREFIX);
    REQUIRE(logFilesNames.size() == 1);

    std::ifstream myfile("/tmp/" + logFilesNames[0]);
    auto opened = myfile.is_open();
    REQUIRE(opened);
    if (opened) {
        getline(myfile, line);
        std::cout << line << std::endl;
        REQUIRE(line == str);
        myfile.close();
    }
}

TEST_CASE("testCreateDir")
//void testCreateDir()
{
    std::string rmStr = "exec rm -rf /tmp/bbb";
    system(rmStr.c_str());
    MetricsFileOutput metricsFileOutput("/tmp/bbb", 10 * 1024, 5, "00001", 10*1024*1024);

    REQUIRE(0 == access("/tmp/bbb/", R_OK));
    sleep(2);
}

TEST_CASE("testMetricsFileRolling")
//void testMetricsFileRolling()
{
    std::string rmStr = "exec rm -rf /tmp/" + ALL_METRIC_PREFIX + "*";
    system(rmStr.c_str());
    MetricsFileOutput metricsFileOutput("/tmp/", 10 * 1024, 5, "00001", 10*1024*1024);
    std::string str = "1234567890"; // 10 bytes

    // 1 files
    std::vector<std::string> metricsVector;
    for (int i=0; i<1023; ++i) {
        metricsVector.push_back(str);
    }
    metricsFileOutput.out(metricsVector);
    metricsFileOutput.flush();
    std::vector<std::string> logFilesNames;
    testListMetricsFile(logFilesNames, TEST_METRIC_PREFIX);
    REQUIRE(logFilesNames.size() == 1);
    sleep(2);

    // 2 files
    std::vector<std::string> metricsVector1;
    metricsVector1.push_back(str);
    metricsVector1.push_back(str);
    metricsVector1.push_back(str);
    metricsFileOutput.out(metricsVector1);
    metricsFileOutput.flush();
    metricsFileOutput.out(metricsVector1);
    metricsFileOutput.flush();
    std::vector<std::string> logFilesNames1;
    testListMetricsFile(logFilesNames1, TEST_METRIC_PREFIX);
    REQUIRE(logFilesNames1.size() == 2);
    sleep(2);

    // 3 files
    metricsFileOutput.out(metricsVector);
    metricsFileOutput.flush();
    metricsFileOutput.out(metricsVector1);
    metricsFileOutput.flush();
    std::vector<std::string> logFilesNames2;
    testListMetricsFile(logFilesNames2, TEST_METRIC_PREFIX);
    REQUIRE(logFilesNames2.size() == 3);
}

TEST_CASE("testMetricsFileDelete")
//void testMetricsFileDelete()
{
    std::string rmStr = "exec rm -rf /tmp/" + ALL_METRIC_PREFIX + "*";
    system(rmStr.c_str());
    MetricsFileOutput metricsFileOutput("/tmp/", 10 * 1024, 5, "00001", 10*1024*1024);
    MetricsFileOutput metricsFileOutput2("/tmp/", 10 * 1024, 5, "00002", 10*1024*1024);

    std::string str = "1234567890"; // 10 bytes

    std::vector<std::string> metricsVector;
    for (int j=0; j<10; ++j) {
        for (int i=0; i<1200; ++i) {
            metricsVector.push_back(str);
        }
        metricsFileOutput.out(metricsVector);
        metricsFileOutput2.out(metricsVector);
        sleep(2);
    }

    metricsFileOutput.flush();
    metricsFileOutput2.flush();

    std::vector<std::string> logFilesNames;
    testListMetricsFile(logFilesNames, TEST_METRIC_PREFIX);
    REQUIRE(logFilesNames.size() == 5);

    logFilesNames.clear();
    testListMetricsFile(logFilesNames, ALL_METRIC_PREFIX + "00002_");
    REQUIRE(logFilesNames.size() == 5);
}


//TEST_CASE("testConfigWrong")
void testConfigWrong()
{
    MetricsFileOutput metricsFileOutput("/tmp/", 10 * 1024, 5, "001", 10*1024*1024);
}
