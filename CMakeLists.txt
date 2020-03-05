cmake_minimum_required(VERSION 3.0)

if (POLICY CMP0054)
    cmake_policy(SET CMP0054 NEW)
endif ()
if (POLICY CMP0057)
    cmake_policy(SET CMP0057 NEW)
endif ()

set(PACKAGE_NAME "bcm-server")
set(PACKAGE_VERSION "0.1.0-dev")
set(PACKAGE_STRING    "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_TARNAME   "${PACKAGE_NAME}-${PACKAGE_VERSION}")

set(CMAKE_CXX_COMPILER "/usr/bin/g++")
project(${PACKAGE_NAME} CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options("-g" "-Wall" "-Wextra" "$<$<CONFIG:RELEASE>:-O2>")

set(TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE PROTO_FILES bcm-protos/proto *.proto)
set(PROTO_FLAGS -I${CMAKE_CURRENT_SOURCE_DIR}/bcm-protos)

add_subdirectory(metrics_sdk)
add_subdirectory(src)
add_subdirectory(tools)

enable_testing()
add_subdirectory(tests)
