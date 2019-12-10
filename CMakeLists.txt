cmake_minimum_required(VERSION 2.8)
PROJECT(my_sylar)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function")
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_AUTOMOC ON)
#et(CMAKE_AUTORCC ON)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
#SET(CMAKE_CXX_COMPILER "/usr/local/bin/g++")

include_directories("./"
        "./tests"
        )

#link_libraries("yaml-cpp") #use find_library replace
find_library(YAMLCPP yaml-cpp /usr/local/lib/)

set(LIB_SRC
        log.cc
        config.cc
        util.cc
        )

add_library(${PROJECT_NAME} SHARED ${LIB_SRC})

add_executable(log_test tests/log_test.cc)
add_dependencies(log_test ${PROJECT_NAME})
target_link_libraries(log_test ${PROJECT_NAME})

add_executable(config_test tests/config_test.cc)
add_dependencies(config_test ${PROJECT_NAME})
target_link_libraries(config_test ${PROJECT_NAME} ${YAMLCPP})
