include_directories(${TOP_DIR}/src)
include_directories(${TOP_DIR}/metrics_sdk/include)

add_definitions("-DAPNS_TEST")
add_definitions("-DNOISE_TEST")
add_definitions("-DUNIT_TEST")
add_definitions(-DRESOURCE_DIR="${TOP_DIR}/resource")

add_library(testing STATIC
        ${HTTP_SOURCE}
        ${WEBSOCKET_SOURCE}
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
        ${OFFLINE_SOURCE}
        ${DISPATCH_SOURCE})

file(GLOB_RECURSE TEST_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR} *.cpp)

foreach(_test_file ${TEST_SRC_FILES})
    get_filename_component(_test_name ${_test_file} NAME_WE)
    add_executable(${_test_name} ${_test_file})
    target_link_libraries(${_test_name} testing ${DEPENDENT_LIBS})
    add_test(${_test_name} ${_test_name})
    set_tests_properties(${_test_name} PROPERTIES TIMEOUT 5)
endforeach()

