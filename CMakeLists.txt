cmake_minimum_required(VERSION 2.8)
PROJECT(my_sylar)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function")
#export GLIBCXX_FORCE_NEW=1
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_AUTOMOC ON)
#et(CMAKE_AUTORCC ON)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
#SET(CMAKE_CXX_COMPILER "/usr/local/bin/g++")

include_directories("./"
        "./tests"
        )
#link_directories("")

find_library(YAMLCPP yaml-cpp /usr/local/lib/)
find_library(THREAD pthread /usr/lib64/)
find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
endif()

set(LIBS ${PROJECT_NAME}
        pthread
        yaml-cpp
        dl
        ${OPENSSL_LIBRARIES}
        )


set(LIB_SRC
        log.cc
        config.cc
        thread.cc
        fiber.cc
        scheduler.cc
        iomanager.cc
        timer.cc
        address.cc
        hook.cc
        fd_manager.cc
        socket.cc
        bytearray.cc
        util.cc
        http/http.cc
        http/dbg.c
        http/http11_parser.cc
        http/httpclient_parser.cc
        http/http_parser.cc
        buffer.cc
        tcp_server.cc
        stream.cc
        http/http_server.cc
        http/http_connection.cc
        uri.cc
        daemon.cc
        env.cc
        hash.cc
        http/ws_connection.cc
        http/ws_server.cc
        )

add_library(${PROJECT_NAME} SHARED ${LIB_SRC})

add_executable(log_test tests/log_test.cc)
add_dependencies(log_test ${PROJECT_NAME})
target_link_libraries(log_test ${LIBS})

add_executable(config_test tests/config_test.cc)
add_dependencies(config_test ${PROJECT_NAME})
target_link_libraries(config_test ${LIBS})

add_executable(thread_test tests/thread_test.cc)
add_dependencies(thread_test ${PROJECT_NAME})
target_link_libraries(thread_test ${LIBS})

add_executable(fiber_test tests/fiber_test.cc)
add_dependencies(fiber_test ${PROJECT_NAME})
target_link_libraries(fiber_test ${LIBS})

add_executable(scheduler_test tests/scheduler_test.cc)
add_dependencies(scheduler_test ${PROJECT_NAME})
target_link_libraries(scheduler_test ${LIBS})

add_executable(iomanager_test tests/iomanager_test.cc)
add_dependencies(iomanager_test ${PROJECT_NAME})
target_link_libraries(iomanager_test ${LIBS})

add_executable(timer_test tests/timer_test.cc)
add_dependencies(timer_test ${PROJECT_NAME})
target_link_libraries(timer_test ${LIBS})

add_executable(pingpong_test tests/pingpong_bench.cc)
add_dependencies(pingpong_test ${PROJECT_NAME})
target_link_libraries(pingpong_test ${LIBS})

add_executable(address_test tests/address_test.cc)
add_dependencies(address_test ${PROJECT_NAME})
target_link_libraries(address_test ${LIBS})

add_executable(hook_test tests/hook_test.cc)
add_dependencies(hook_test ${PROJECT_NAME})
target_link_libraries(hook_test ${LIBS})

add_executable(socket_test tests/socket_test.cc)
add_dependencies(socket_test ${PROJECT_NAME})
target_link_libraries(socket_test ${LIBS})

add_executable(bytearray_test tests/bytearray_test.cc)
add_dependencies(bytearray_test ${PROJECT_NAME})
target_link_libraries(bytearray_test ${LIBS})

add_executable(http_test tests/http_test.cc)
add_dependencies(http_test ${PROJECT_NAME})
target_link_libraries(http_test ${LIBS})

add_executable(server_test tests/server_test.cc)
add_dependencies(server_test ${PROJECT_NAME})
target_link_libraries(server_test ${LIBS})

add_executable(server_test1 tests/server_test1.cc)
add_dependencies(server_test1 ${PROJECT_NAME})
target_link_libraries(server_test1 ${LIBS})

add_executable(echo_server examples/echo_server.cc)
add_dependencies(echo_server ${PROJECT_NAME})
target_link_libraries(echo_server ${LIBS})

add_executable(http_server tests/http_server.cc)
add_dependencies(http_server ${PROJECT_NAME})
target_link_libraries(http_server ${LIBS})

add_executable(race_condition tests/race_condition.cc)
add_dependencies(race_condition ${PROJECT_NAME})
target_link_libraries(race_condition ${LIBS})

add_executable(http_connection_test tests/http_connection_test.cc)
add_dependencies(http_connection_test ${PROJECT_NAME})
target_link_libraries(http_connection_test ${LIBS})

add_executable(http_parser tests/http_parser.cc)
add_dependencies(http_parser ${PROJECT_NAME})
target_link_libraries(http_parser ${LIBS})

add_executable(uri_test tests/uri_test.cc)
add_dependencies(uri_test ${PROJECT_NAME})
target_link_libraries(uri_test ${LIBS})

add_executable(tcp_proxy_test tests/tcp_proxy_test.cc)
add_dependencies(tcp_proxy_test ${PROJECT_NAME})
target_link_libraries(tcp_proxy_test ${LIBS})

add_executable(udp_test tests/udp_test.cc)
add_dependencies(udp_test ${PROJECT_NAME})
target_link_libraries(udp_test ${LIBS})

add_executable(daemon_test tests/daemon_test.cc)
add_dependencies(daemon_test ${PROJECT_NAME})
target_link_libraries(daemon_test ${LIBS})

add_executable(env_test tests/env_test.cc)
add_dependencies(env_test ${PROJECT_NAME})
target_link_libraries(env_test ${LIBS})

add_executable(ws_test tests/ws_test.cc)
add_dependencies(ws_test ${PROJECT_NAME})
target_link_libraries(ws_test ${LIBS})

###############################
add_executable(counter utils/counter.cc)
add_dependencies(counter ${PROJECT_NAME})
target_link_libraries(counter ${LIBS})

#valgrind --tool=memcheck --track-origins=yes --leak-check=full --show-leak-kinds=all ./scheduler_test
