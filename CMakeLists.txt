cmake_minimum_required(VERSION 3.7)
PROJECT(my_sylar)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(cmake/utils.cmake)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined")
#set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined")
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
enable_language(ASM)

set(CMAKE_INSTALL_PREFIX /usr/local)
set(INSTALL_PATH_LIB lib)
set(INSTALL_PATH_INCLUDE include)
set(SUB_DIR_LIST "my_sylar/utils" "my_sylar/ns" "my_sylar/http" "my_sylar/fcontext")

include_directories("./"
        "./tests"
        )

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
        my_sylar/log.cc
        my_sylar/config.cc
        my_sylar/thread.cc
        my_sylar/fiber.cc
        my_sylar/scheduler.cc
        my_sylar/iomanager.cc
        my_sylar/timer.cc
        my_sylar/address.cc
        my_sylar/hook.cc
        my_sylar/fd_manager.cc
        my_sylar/socket.cc
        my_sylar/bytearray.cc
        my_sylar/buffer.cc
        my_sylar/mbuffer.cc
        my_sylar/util.cc
        my_sylar/http/http.cc
        my_sylar/http/dbg.c
        my_sylar/http/http11_parser.cc
        my_sylar/http/httpclient_parser.cc
        my_sylar/http/http_parser.cc
        my_sylar/tcp_server.cc
        my_sylar/stream.cc
        my_sylar/http/http_server.cc
        my_sylar/http/http_connection.cc
        my_sylar/uri.cc
        my_sylar/daemon.cc
        my_sylar/hash.cc
        my_sylar/http/ws_connection.cc
        my_sylar/http/ws_server.cc
        my_sylar/worker.cc
        my_sylar/application.cc
        my_sylar/module.cc
        my_sylar/fcontext/fcontext.S
        my_sylar/utils/crypto_utils.cc
        my_sylar/dns.cc
        my_sylar/ns/ares.cc
        my_sylar/socks.cc
        my_sylar/utils/ikcp.c
        )

add_library(${PROJECT_NAME} SHARED ${LIB_SRC})
install(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION ${INSTALL_PATH_LIB} LIBRARY DESTINATION ${INSTALL_PATH_LIB})

add_library(${PROJECT_NAME}_static STATIC ${LIB_SRC})
install(TARGETS ${PROJECT_NAME}_static ARCHIVE DESTINATION ${INSTALL_PATH_LIB})

force_redefine_file_macro_for_sources(${PROJECT_NAME}) #__FILE__

option(TEST "ON for complile test" OFF)
if(TEST)
my_sylar_add_executable(counter my_sylar/utils/counter.cc ${PROJECT_NAME} "${LIBS}")
endif()
my_sylar_add_executable(echo_test examples/echo_server.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(ares_test tests/ares_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(tunnel_test tests/tunnel_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(uri_test tests/uri_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(lru_test tests/lru_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(pingpong_qps_test my_sylar/utils/pingpong_qps.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(kcp_test tests/kcp_test.cc ${PROJECT_NAME} "${LIBS}")
my_sylar_add_executable(port_scan tests/port_scan.cc ${PROJECT_NAME} "${LIBS}")

install(DIRECTORY my_sylar DESTINATION ${INSTALL_PATH_INCLUDE} FILES_MATCHING PATTERN "*.hh")

set(CPACK_PACKAGE_NAME "my_sylar")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "my_sylar")
set(CPACK_PACKAGE_VENDOR "hankai17")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_RPM_PACKAGE_GROUP "hankai17")
set(CPACK_RPM_PACKAGE_URL "http://github.com/hankai17/my_sylar")
set(CPACK_RPM_PACKAGE_DESCRIPTION "")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_RPM_PACKAGE_LICENSE "ASL 2.0")
set(CPACK_GENERATOR "RPM")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local/")
include(CPack)

