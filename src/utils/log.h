#pragma once

#include <string>
#include <iostream>
#include <boost/log/common.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include "fiber/fiber_pool.h"
#include <config/log_config.h>

namespace bcm {
    
    template<class T1, class T2>
    std::string toString(const std::map< T1, T2>& src)
    {
        std::stringstream ss;
        std::string sep;
        for (const auto& it : src) {
            ss << sep << "<" << it.first << "," << it.second << ">";
            sep = ",";
        }
        return ss.str();
    }
    
    template<class T>
    std::string toString(const std::set< T >& src)
    {
        std::stringstream ss;
        std::string sep;
        for (const auto& it : src) {
            ss << sep << "<" << it << ">";
            sep = ",";
        }
        return ss.str();
    }
    
    template<class T>
    std::string toString(const std::vector< T >& src)
    {
        std::stringstream ss;
        std::string sep;
        for (const auto& it : src) {
            ss << sep << "<" << it << ">";
            sep = ",";
        }
        return ss.str();
    }
    
enum LogSeverity {
    LOGSEVERITY_TRACE = 0,
    LOGSEVERITY_DEBUG = 1,
    LOGSEVERITY_INFO = 2,
    LOGSEVERITY_WARN = 3,
    LOGSEVERITY_ERROR = 4,
    LOGSEVERITY_FATAL = 5
};

class Log {
public:
    static bool init(LogConfig& conf, const char* fileHead);
    static void flush();
};

//BOOST_LOG_ATTRIBUTE_KEYWORD(lineId, "LineID", unsigned int)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", LogSeverity)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::log::attributes::local_clock::value_type)
//BOOST_LOG_ATTRIBUTE_KEYWORD(processId, "ProcessID", boost::log::attributes::current_process_id::value_type)
BOOST_LOG_ATTRIBUTE_KEYWORD(threadId, "ThreadID", boost::log::attributes::current_thread_id::value_type)

BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(BcmLogger, boost::log::sources::severity_logger_mt<LogSeverity>);

#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

#define BCMLOG(severity)\
    BOOST_LOG_SEV(BcmLogger::get(), LOGSEVERITY_##severity)\
    << boost::this_fiber::get_id() << "|" << __FILENAME__ << ":" <<  __PRETTY_FUNCTION__ << ":" << __LINE__ << "|"

#define LOG2(severity)\
    BOOST_LOG_SEV(BcmLogger::get(), severity)\
    << boost::this_fiber::get_id() << "|" << __FILENAME__ << ":" <<  __PRETTY_FUNCTION__ << ":" << __LINE__ << "|"

#define LOGT BCMLOG(TRACE)
#define LOGD BCMLOG(DEBUG)
#define LOGI BCMLOG(INFO)
#define LOGW BCMLOG(WARN)
#define LOGE BCMLOG(ERROR)
#define LOGF BCMLOG(FATAL)

}
