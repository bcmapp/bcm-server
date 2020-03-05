#pragma once

#include <sys/time.h>
#include <chrono>

namespace bcm {
namespace metrics {

class MetricsCommon
{
public:
    static int64_t nowInMilli()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>
                (std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

} //namespace
}