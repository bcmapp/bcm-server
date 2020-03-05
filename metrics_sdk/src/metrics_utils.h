#pragma once

// only for unix/linux

#include <pthread.h>
#include <string>

namespace bcm {
namespace metrics {

static inline void setThreadName(const std::string& name)
{
    // max name size if 15
    if (name.size() > 15) {
        pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
    } else {
        pthread_setname_np(pthread_self(), name.c_str());
    }
}

} //namespace
}