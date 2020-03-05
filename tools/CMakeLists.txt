include_directories(${TOP_DIR}/src /usr/include/python3.5)
include_directories(${TOP_DIR}/metrics_sdk/include)

add_library(tooltesting STATIC
        ${UTILS_SOURCE}
        ${CONTROLLERS_SOURCE}
        ${FIBER_SOURCE}
        ${PROTO_SOURCE}
        ${AUTH_SOURCE}
        ${CRYPTO_SOURCE}
        ${HIREDIS_SOURCE}
        ${GROUP_SOURCE}
        ${STORE_SOURCE}
        ${BLOOM_SOURCE}
        ${FEATURES_SOURCE}
        ${PUSH_SOURCE}
        ${LIMITERS_SOURCE}
        ${DAO_SOURCE}
        ${METRICS_SOURCE}
        ${DISPATCH_SOURCE})

add_executable(account_tool account_tool.cpp)
target_link_libraries(account_tool libboost_program_options.a libbitcoincrypto.a libsignal-protocol-c.a ssl crypto)

add_executable(device_tool
    device_tool.cpp
    ${TOP_DIR}/src/proto/device/multi_device.pb.cc)
target_link_libraries(device_tool ${DEPENDENT_LIBS})

add_executable(simple_client simple_client.cpp)
target_link_libraries(simple_client libboost_program_options.a libboost_system.a ssl crypto pthread)

add_executable(bloom_filters_tool
    bloom_filters_tool.cpp
    ${TOP_DIR}/src/crypto/base64.cpp
    ${TOP_DIR}/src/bloom/bloom_filters.cpp
    ${TOP_DIR}/src/bloom/hash_algo.cpp
    ${TOP_DIR}/src/crypto/murmurhash3.cpp)
target_link_libraries(bloom_filters_tool libboost_program_options.a)

set(PYTHON_MODULE "false")
if(PYTHON_MODULE MATCHES "true")
    add_library(signup SHARED signupmodule.cpp signup_tool.cpp)
    set(signup CMAKE_CXX_FLAGS -fPIC)
    set_target_properties(signup PROPERTIES PREFIX "")
    target_link_libraries(signup libboost_program_options.a libbitcoincrypto.a libsignal-protocol-c.a ssl crypto)
endif()

