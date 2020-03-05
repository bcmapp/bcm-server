#include "../include/metrics_client.h"
#include "metrics_test_common.h"
#include "test_common.h"
#include <chrono>

#include <vector>
#include <string>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <algorithm>
#include <dirent.h>
#include <unistd.h>

using namespace bcm::metrics;

void testGetMetricsConfig(MetricsConfig& config) {
    config.appVersion = "1.0";
    config.reportQueueSize = 5000;
    config.metricsDir = "/tmp";
    config.metricsFileSizeInBytes = 1024*10;
    config.metricsFileCount = 5;
    config.reportIntervalInMs = 3000;
    config.clientId = "00001";
    config.writeThresholdInBytes = 1024 * 1024 * 100;
}

TEST_CASE("testMetricsClient")
//void testMetricsClient()
{
    std::string rmStr = "exec rm -rf /tmp/" + ALL_METRIC_PREFIX + "*";
    system(rmStr.c_str());
    MetricsConfig config;
    testGetMetricsConfig(config);
    MetricsClient::Init(config);
    MetricsClient::Instance()->markMicrosecondAndRetCode("testapp","testtopic",10000, "200");
    MetricsClient::Instance()->markMicrosecondAndRetCode("testapp","testtopic",20000, "300");
    MetricsClient::Instance()->markMicrosecondAndRetCode("testapp","testtopic",10000, "300");
    MetricsClient::Instance()->markMicrosecondAndRetCode("testapp","testtopic",40000, "200");
    MetricsClient::Instance()->markMicrosecondAndRetCode("testapp","testtopic",20000, "201");
    MetricsClient::Instance()->markMicrosecondAndRetCode("testapp","testtopic",20000, "201");
    MetricsClient::Instance()->markMicrosecondAndRetCode("testapp","testtopic",40000, "201");

    MetricsClient::Instance()->directOutput("directop1","directop1-c1");
    MetricsClient::Instance()->directOutput("directop1","directop1-c2");
    MetricsClient::Instance()->directOutput("directop1","directop1-c3");
    MetricsClient::Instance()->directOutput("directop2","directop1-aaaaa");

    for (int i=0; i<2000; i++) {
        MetricsClient::Instance()->markMicrosecondAndRetCode("testapp1","testtopic1",10000, "200");
    }

    MetricsClient::Instance()->counterAdd("counter1", 1);
    MetricsClient::Instance()->counterAdd("counter1", 2);
    MetricsClient::Instance()->counterSet("counter2", 100);
    MetricsClient::Instance()->counterSet("counter2", 200);
    MetricsClient::Instance()->counterSet("counter2", 300);
    MetricsClient::Instance()->counterAdd("counter3", 100);

    sleep(7);

    std::vector<std::string> logFilesNames;
    std::string line;
    testListMetricsFile(logFilesNames, TEST_METRIC_PREFIX);

    std::ifstream myfile("/tmp/" + logFilesNames[0]);
    auto opened = myfile.is_open();
    REQUIRE(opened);
    if (opened) {

        // mix result
        line = "";
        getline(myfile, line);
        std::cout << line << std::endl;
        REQUIRE(line.substr(18) == "testapp1,testtopic1,1.0,2000,200,10000");

        line = "";
        getline(myfile, line);
        std::cout << line << std::endl;
        REQUIRE(line.substr(18) == "testapp,testtopic,1.0,2,200,22857");

        line = "";
        getline(myfile, line);
        std::cout << line << std::endl;
        REQUIRE(line.substr(18) == "testapp,testtopic,1.0,3,201,22857");

        line = "";
        getline(myfile, line);
        std::cout << line << std::endl;
        REQUIRE(line.substr(18) == "testapp,testtopic,1.0,2,300,22857");

        // counter result
        line = "";
        getline(myfile, line);
        std::cout << line << std::endl;
        REQUIRE(line.substr(0, 8) == "counter1");
        REQUIRE(line.substr(23) == "3");

        line = "";
        getline(myfile, line);
        std::cout << line << std::endl;
        REQUIRE(line.substr(0, 8) == "counter2");
        REQUIRE(line.substr(23) == "300");

        line = "";
        getline(myfile, line);
        std::cout << line << std::endl;
        REQUIRE(line.substr(0, 8) == "counter3");
        REQUIRE(line.substr(23) == "100");

        // direct output
        line = "";
        getline(myfile, line);
        std::cout << line << std::endl;
        REQUIRE(line.substr(0, 9) == "directop1");
        REQUIRE(line.substr(24) == "directop1-c1");

        line = "";
        getline(myfile, line);
        std::cout << line << std::endl;
        REQUIRE(line.substr(0, 9) == "directop1");
        REQUIRE(line.substr(24) == "directop1-c2");

        line = "";
        getline(myfile, line);
        std::cout << line << std::endl;
        REQUIRE(line.substr(0, 9) == "directop1");
        REQUIRE(line.substr(24) == "directop1-c3");

        line = "";
        getline(myfile, line);
        std::cout << line << std::endl;
        REQUIRE(line.substr(0, 9) == "directop2");
        REQUIRE(line.substr(24) == "directop1-aaaaa");

        myfile.close();
    }
}

//TEST_CASE("testMetricsClientLoadTest")
void testMetricsClientLoadTest()
{
    std::string rmStr = "exec rm -rf /tmp/" + ALL_METRIC_PREFIX + "*";
    system(rmStr.c_str());
    MetricsConfig config;
    testGetMetricsConfig(config);
    MetricsClient::Init(config);
    std::vector<std::thread> threads;

    int totalMetricsDimension = 1000;
    std::string retCodes[totalMetricsDimension];
    std::string topics[totalMetricsDimension];
    for (int j=0; j<totalMetricsDimension; ++j) {
        retCodes[j] = std::to_string(j+300);
        topics[j] = "topic" + std::to_string(j);
    }

    // 7 mix
    for (int i=0; i<7; ++i) {
        threads.push_back(std::thread([&](){
            uint64_t index = 1;
            while(true){
                MetricsClient::Instance()->markMicrosecondAndRetCode("testapp",
                                                                     topics[index % totalMetricsDimension],
                                                                     (int64_t)index % totalMetricsDimension,
                                                                     retCodes[index % totalMetricsDimension]);
                index++;
            }
        }));
    }

    // 3 counter
    for (int i=0; i<3; ++i) {
        threads.push_back(std::thread([&](){
            uint64_t index = 1;
            while(true){
                if (index % 2 == 0) {
                    MetricsClient::Instance()->counterAdd(topics[index % totalMetricsDimension], 1);
                } else {
                    MetricsClient::Instance()->counterSet(topics[index % totalMetricsDimension], (int64_t) index);
                }
                index++;
            }
        }));
    }

    // 3 direct output
    for (int i=0; i<3; ++i) {
        threads.push_back(std::thread([&](){
            uint64_t index = 1;
            while(true){
                MetricsClient::Instance()->directOutput(topics[index % totalMetricsDimension],
                                                        retCodes[index % totalMetricsDimension]);
                index++;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }));
    }

    for (int i=0; i<13; ++i) {
        threads[i].detach();
    }

    sleep(1800);
    for (int i=0; i<13; ++i) {
        pthread_cancel(threads[i].native_handle());
    }
    sleep(5);
}

