#pragma once

#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>
#include <iostream>

class TestLogger {
public:
    explicit TestLogger(std::ostream& ostream = std::cerr)
        : m_ostream(ostream)
    {
    }

    template<typename T>
    TestLogger& operator<<(const T& t)
    {
        m_ostream << t;
        return *this;
    }

    ~TestLogger()
    {
        m_ostream << std::endl;
    }

    std::ostream& m_ostream;
};

#define TLOG TestLogger()
