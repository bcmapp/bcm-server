cmake_minimum_required(VERSION 3.5)
project(bcm_metrics CXX)

option(BUILD_METRICS_SDK_TESTS "Build test programs" OFF)

SET(CMAKE_CXX_COMPILER "/usr/bin/g++")
SET(CMAKE_C_COMPILER "/usr/bin/gcc")
SET(CMAKE_CXX_STANDARD 14)
SET(METRICS_SDK_TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE METRICS_SDK_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(metrics_sdk STATIC
        ${METRICS_SDK_SOURCE})

if (BUILD_METRICS_SDK_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif (BUILD_METRICS_SDK_TESTS)
