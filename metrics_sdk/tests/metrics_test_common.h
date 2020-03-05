#pragma once

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <dirent.h>
#include <string>
#include <unistd.h>
#include "test_common.h"

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
