#pragma once

#include <sstream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdio.h>
#include <iostream>
#include "../include/metrics_log.h"

#define METRICS_LOG_INFO(format, args...) do { \
            auto t = std::time(nullptr); \
            char buffer[2048] = {0}; \
            snprintf(buffer, 2048, format, ##args); \
            if (bcm::metrics::MetricsLogger::instance()->isLog()) {  \
                std::stringstream ssLog; \
                ssLog << "[" << __FUNCTION__ << "](" << __FILE__ << ":" << __LINE__ << ") " << buffer; \
                bcm::metrics::MetricsLogger::instance()->print(bcm::metrics::METRICS_LOGLEVEL_INFO, ssLog.str()); \
            } else { \
                std::cout << "[INFO]\t" \
                << std::put_time(std::gmtime(&t), "%c %Z") \
                << " [" << __FUNCTION__ << "](" << __FILE__ << ":" << __LINE__ << ") " \
                << buffer \
                << std::endl; \
            } \
        } while (0)

#define METRICS_LOG_ERR(format, args...) do { \
            auto t = std::time(nullptr); \
            char buffer[2048] = {0}; \
            snprintf(buffer, 2048, format, ##args); \
            if (bcm::metrics::MetricsLogger::instance()->isLog()) {  \
                std::stringstream ssLog; \
                ssLog << "[" << __FUNCTION__ << "](" << __FILE__ << ":" << __LINE__ << ") " << buffer; \
                bcm::metrics::MetricsLogger::instance()->print(bcm::metrics::METRICS_LOGLEVEL_ERROR, ssLog.str()); \
            } else { \
                std::cout << "[ERROR]\t" \
                << std::put_time(std::gmtime(&t), "%c %Z") \
                << " [" << __FUNCTION__ << "](" << __FILE__ << ":" << __LINE__ << ") " \
                << buffer \
                << std::endl; \
            } \
        } while (0)

#define METRICS_LOG_TRACE(format, args...) do { \
            auto t = std::time(nullptr); \
            char buffer[2048] = {0}; \
            snprintf(buffer, 2048, format, ##args); \
            if (bcm::metrics::MetricsLogger::instance()->isLog()) {  \
                std::stringstream ssLog; \
                ssLog << "[" << __FUNCTION__ << "](" << __FILE__ << ":" << __LINE__ << ") " << buffer; \
                bcm::metrics::MetricsLogger::instance()->print(bcm::metrics::METRICS_LOGLEVEL_TRACE, ssLog.str()); \
            } else { \
                std::cout << "[TRACE]\t" \
                << std::put_time(std::gmtime(&t), "%c %Z") \
                << " [" << __FUNCTION__ << "](" << __FILE__ << ":" << __LINE__ << ") " \
                << buffer \
                << std::endl; \
            } \
        } while (0)
