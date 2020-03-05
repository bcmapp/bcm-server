include_directories(${TOP_DIR}/src)
include_directories(${TOP_DIR}/metrics_sdk/include)

option(APNS_SUBMIT_FAILURE_TEST "Build apns request submitting failure test" OFF)
option(GROUP_EXCEPTION_INJECT_TEST "Build group failure test" OFF)

if(APNS_SUBMIT_FAILURE_TEST)
    message(WARNING "APNS_SUBMIT_FAILURE_TEST is ON")
    add_definitions("-DAPNS_SUBMIT_FAILURE_TEST")
else()
    message(STATUS "APNS_SUBMIT_FAILURE_TEST is OFF")
    remove_definitions("-DAPNS_SUBMIT_FAILURE_TEST")
endif()

if(GROUP_EXCEPTION_INJECT_TEST)
    message(WARNING "GROUP_EXCEPTION_INJECT_TEST is ON")
    add_definitions("-DGROUP_EXCEPTION_INJECT_TEST")
else()
    message(STATUS "GROUP_EXCEPTION_INJECT_TEST is OFF")
    remove_definitions("-DGROUP_EXCEPTION_INJECT_TEST")
endif()

# protobuf
find_package(Protobuf REQUIRED)
foreach(_proto_file ${PROTO_FILES})
    execute_process(
            COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} ${PROTO_FLAGS} --cpp_out=${CMAKE_CURRENT_SOURCE_DIR} ${_proto_file}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()
file(GLOB_RECURSE PROTO_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/proto *.cc)
set(PROTO_SOURCE ${PROTO_SOURCE} CACHE INTERNAL "Protobuf Source Files")

set(CONFIG_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/config/bcm_options.cpp
        CACHE INTERNAL "Utils Source Files")

set(UTILS_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/log.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/ssl_utils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/account_helper.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/number_utils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/libevent_utils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/sender_utils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/lease_utils.cpp
        CACHE INTERNAL "Utils Source Files")

set(FIBER_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/fiber/asio_round_robin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/fiber/asio_yield.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/fiber/fiber_pool.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/fiber/fiber_timer.cpp
        CACHE INTERNAL "Fiber Source Files")

set(HTTP_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/http/http_service.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/http/custom_http_status.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/http/http_route.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/http/http_router.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/http/http_session.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/http/http_statics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/http/http_url.cpp
        CACHE INTERNAL "HTTP Source Files")

set(WEBSOCKET_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/websocket/websocket_service.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/websocket/websocket_session.cpp
        CACHE INTERNAL "WebSocket Source Files")

set(CONTROLLERS_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/echo_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/account_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/device_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/message_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/group_msg_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/keys_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/keep_alive_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/device_keepalive_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/contact_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/profile_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/attachment_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/group_manager_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/group_manager_controller_V3.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/group_manager_entities.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/opaque_data_controller.cpp
        CACHE INTERNAL "Controllers Source Files")

set(DAO_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/rpc_impl/contacts_rpc_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/rpc_impl/onetime_keys_rpc_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/rpc_impl/signup_challenges_rpc_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/rpc_impl/accounts_rpc_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/rpc_impl/stored_messages_rpc_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/rpc_impl/group_user_rpc_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/rpc_impl/group_rpc_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/rpc_impl/group_msg_rpc_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/rpc_impl/limiters_rpc_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/rpc_impl/sys_messages_rpc_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/rpc_impl/utilities_rpc_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/rpc_impl/pending_group_user_rpc_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/rpc_impl/limiter_configurations_rpc_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/rpc_impl/group_keys_rpc_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/rpc_impl/qr_code_group_users_rpc_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/rpc_impl/opaque_data_rpc_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/dao_cache/redis_cache.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dao/client.cpp
        CACHE INTERNAL "Dao Source Files")

set(HIREDIS_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/redis/hiredis_client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/redis/async_conn.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/redis/reply.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/redis/redis_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/redis/redis_manage_timer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/redis/online_redis_manager.cpp
        CACHE INTERNAL "Hiredis client Source Files")

set(METRICS_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/metrics/onlineuser_metrics.cpp
        CACHE INTERNAL "Metrics Source Files")

set(DISPATCH_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/dispatcher/dispatch_address.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dispatcher/dispatch_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dispatcher/dispatch_channel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dispatcher/offline_dispatcher.cpp
        CACHE INTERNAL "Dispatch Manager Source Files")

set(STORE_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/store/messages_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/store/accounts_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/store/keys_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/store/limiters_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/store/contact_token_manager.cpp
        CACHE INTERNAL "Store Manager Source Files")

set(AUTH_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/auth/authorization_header.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/auth/authenticator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/auth/turntoken_generator.cpp
        CACHE INTERNAL "Auth Source Files")

set(CRYPTO_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/crypto/url_encoder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/crypto/hex_encoder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/crypto/hmac.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp_cipher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sha1.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/crypto/base64.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/crypto/gzip.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/crypto/fnv.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/crypto/murmurhash3.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/crypto/any_base.cpp
        CACHE INTERNAL "Crypto Source Files")

set(PUSH_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/push/push_notification.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/push/apns_notification.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/push/apns_client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/push/apns_service.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/push/apns_qos_mgr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/push/fcm_notification.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/push/fcm_client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/push/umeng_notification.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/push/umeng_client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/push/push_service.cpp
        CACHE INTERNAL "push source files")

set(BLOOM_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/bloom/hash_algo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bloom/bloom_filters.cpp
        CACHE INTERNAL "bloom source files")

set(FEATURES_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/features/base_features.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/features/bcm_features.cpp
        CACHE INTERNAL "features source files")

set(REGISTERS_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/registers/lbs_register.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/registers/imservice_register.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/registers/offline_register.cpp
        CACHE INTERNAL "registers source files")

set(FILTERS_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/filters/magic_status_code_filter.cpp
        CACHE INTERNAL "filters source files")

set(GROUP_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/group/group_event_sub.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/group/group_msg_service.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/group/group_msg_sub.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/group/im_server_mgr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/group/io_ctx_executor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/group/io_ctx_pool.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/group/member_mgr_base.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/group/online_msg_handler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/group/online_msg_member_mgr.cpp
        CACHE INTERNAL "group source files")

set(LIMITERS_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/limiters/distributed_limiter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/limiters/user_qps_limiter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/limiters/api_qps_limiter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/limiters/dependency_limiter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/limiters/limiter_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/limiters/configuration_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/limiters/api_checker.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/limiters/uid_checker.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/limiters/api_matcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/limiters/limiter_config_update.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/limiters/limiter_executor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/limiters/limiter_globals.cpp
        CACHE INTERNAL "limiters source files")

set(BOOST_LIBS
        libboost_system.a
        libboost_fiber.a
        libboost_context.a
        libboost_log.a
        libboost_program_options.a
        libboost_thread.a
        libboost_filesystem.a
        libboost_iostreams.a
        libboost_regex.a
        CACHE INTERNAL "boost libraries")

set(NGHTTP2_LIBS
        libnghttp2_asio.a
        libnghttp2.a
        CACHE INTERNAL "nghttp2 libraries")

set(PHONENUMBER_LIBS
        libphonenumber.a
        libicui18n.a
        libicuuc.a
        libicudata.a)

set(BCMKV_LIBS
        libpqxx.a
        libpq.a
        libaio.a)

set(OTHER_STATIC_LIBS
        libbrpc.a
        libprotobuf.a
        libbitcoincrypto.a
        libsignal-protocol-c.a
        libhiredis.a
        libgflags.a
        libleveldb.a
        libsnappy.a)


set(DEPENDENT_LIBS
        ${PHONENUMBER_LIBS}
        ${BCMKV_LIBS}
        ${NGHTTP2_LIBS}
        ${OTHER_STATIC_LIBS}
        ${BOOST_LIBS}
        metrics_sdk
        z m rt dl ssl crypto event pthread event_pthreads
        CACHE INTERNAL "Dependeent libraries")

add_executable(bcm-server
        ${CMAKE_CURRENT_SOURCE_DIR}/program/bcm-server/main.cpp
        ${CONFIG_SOURCE}
        ${HTTP_SOURCE}
        ${HIREDIS_SOURCE}
        ${WEBSOCKET_SOURCE}
        ${UTILS_SOURCE}
        ${CONTROLLERS_SOURCE}
        ${METRICS_SOURCE}
        ${DISPATCH_SOURCE}
        ${STORE_SOURCE}
        ${FIBER_SOURCE}
        ${PROTO_SOURCE}
        ${AUTH_SOURCE}
        ${CRYPTO_SOURCE}
        ${PUSH_SOURCE}
        ${BLOOM_SOURCE}
        ${FEATURES_SOURCE}
        ${REGISTERS_SOURCE}
        ${FILTERS_SOURCE}
        ${GROUP_SOURCE}
        ${LIMITERS_SOURCE}
        ${DAO_SOURCE})

target_link_libraries(bcm-server ${DEPENDENT_LIBS})

set(OFFLINE_DUPLICATE_BCM_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/group/io_ctx_pool.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/group/io_ctx_executor.cpp
        CACHE INTERNAL "offline duplicate bcm Source Files")

set(OFFLINE_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/registers/offline_register.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/program/offline-server/offline_config.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/program/offline-server/offline_server.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/program/offline-server/groupuser_event_sub.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/program/offline-server/group_member_mgr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/program/offline-server/offline_server_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/program/offline-server/offline_member_mgr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers/system_controller.cpp
        CACHE INTERNAL "offline Source Files")


add_executable(offline-server
        ${UTILS_SOURCE}
        ${DAO_SOURCE}
        ${HIREDIS_SOURCE}
        ${WEBSOCKET_SOURCE}
        ${STORE_SOURCE}
        ${PROTO_SOURCE}
        ${CRYPTO_SOURCE}
        ${METRICS_SOURCE}
        ${FIBER_SOURCE}
        ${PUSH_SOURCE}
        ${AUTH_SOURCE}
        ${HTTP_SOURCE}
        ${DISPATCH_SOURCE}
        ${OFFLINE_DUPLICATE_BCM_SOURCE}
        ${OFFLINE_SOURCE}
        ${CMAKE_CURRENT_SOURCE_DIR}/program/offline-server/main.cpp
        )

target_link_libraries(offline-server ${DEPENDENT_LIBS})
