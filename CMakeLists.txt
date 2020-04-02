cmake_minimum_required(VERSION 3.7)
PROJECT(my_sylar)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(cmake/utils.cmake)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined")
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
enable_language(ASM)

include_directories("./"
        "./tests"
        )

find_library(YAMLCPP yaml-cpp /usr/local/lib/)
find_library(THREAD pthread /usr/lib64/)
find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
endif()

find_package(MySQL REQUIRED)
if(MYSQL_FOUNT)
    include_directories(${MYSQL_INCLUDE_DIR})
endif()

set(LIBS ${PROJECT_NAME}
        pthread
        yaml-cpp
        dl
        ${OPENSSL_LIBRARIES}
        #jemalloc
        ${MYSQL_LIBRARIES}
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
        hash.cc
        http/ws_connection.cc
        http/ws_server.cc
        worker.cc
        application.cc
        module.cc
        fcontext/fcontext.S
        protocol.cc
        utils/crypto_utils.cc
        ns/ns_protocol.cc
        #################
        dns.cc
        rock/rock.cc
        ns/ares.cc
        db/mysql.cc
        )

add_library(${PROJECT_NAME} SHARED ${LIB_SRC})

force_redefine_file_macro_for_sources(${PROJECT_NAME}) #__FILE__

option(TEST "ON for complile test" OFF)
if(TEST)
my_sylar_add_executable(log_test tests/log_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(config_test tests/config_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(thread_test tests/thread_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(fiber_test tests/fiber_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(scheduler_test tests/scheduler_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(iomanager_test tests/iomanager_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(timer_test tests/timer_test.cc ${PROJECT_NAME} "${LIBS}")
#my_sylar_add_executable(pingpong_test tests/pingpong_bench.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(address_test tests/address_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(hook_test tests/hook_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(socket_test tests/socket_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(bytearray_test tests/bytearray_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(http_test tests/http_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(server_test tests/server_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(server_test1 tests/server_test1.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(echo_server examples/echo_server.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(http_server tests/http_server.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(race_condition tests/race_condition.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(http_connection_test tests/http_connection_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(http_parser tests/http_parser.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(uri_test tests/uri_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(tcp_proxy_test tests/tcp_proxy_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(udp_test tests/udp_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(daemon_test tests/daemon_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(ws_test tests/ws_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(application_test tests/application_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(crypto_test tests/crypto_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(counter utils/counter.cc ${PROJECT_NAME} "${LIBS}")
###############################
endif()
my_sylar_add_executable(pingpong_test tests/pingpong_bench.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(normal_test tests/normal_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(rock_test tests/rock_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(singleton_test tests/singleton_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(ares_test tests/ares_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(http_test tests/http_test.cc ${PROJECT_NAME} "${LIBS}")

#valgrind --tool=memcheck --track-origins=yes --leak-check=full --show-leak-kinds=all ./scheduler_test
