#pragma once

#include <sys/time.h>
#include <chrono>

namespace bcm {

static inline int64_t nowInSec()
{
    return std::chrono::duration_cast<std::chrono::seconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();
}

static inline int64_t nowInMilli()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();
}

static inline int64_t nowInMicro()
{
    return std::chrono::duration_cast<std::chrono::microseconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();
}

static inline int64_t nowInNano()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();
}

static inline int64_t todayInMilli()
{
    typedef std::chrono::duration<int64_t, std::ratio<24 * 60 * 60>> days;
    auto durationInDay = std::chrono::duration_cast<days>(std::chrono::system_clock::now().time_since_epoch());
    return std::chrono::duration_cast<std::chrono::milliseconds>(durationInDay).count();
}

inline uint64_t get_current_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec/1000;
}

inline uint64_t get_current_us()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec;
}

static inline int64_t steadyNowInMilli()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::steady_clock::now().time_since_epoch()).count();
}

}