cmake_minimum_required(VERSION 2.8)
PROJECT(my_sylar)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function")
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
SET(CMAKE_CXX_COMPILER "/usr/local/bin/g++")

include_directories("./"
        "./tests"
        )

set(LIB_SRC
        log.cc
        util.cc
        )

add_library(${PROJECT_NAME} SHARED ${LIB_SRC})

add_executable(log_test tests/log_test.cc)
add_dependencies(log_test ${PROJECT_NAME})
target_link_libraries(log_test ${PROJECT_NAME})

