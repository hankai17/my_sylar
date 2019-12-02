cmake_minimum_required(VERSION 3.2)
PROJECT(my_sylar)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")

SET(CMAKE_CXX_COMPILER "/usr/local/bin/g++")
#FIND_LIBRARY(
#        LIBDB_CXX_LIBRARIES NAMES  boost_regex
#        PATHS /usr/lib/ /usr/local/lib/
#)

add_executable(${PROJECT_NAME}
        log.h
        log.cpp
        )

#TARGET_LINK_LIBRARIES(${PROJECT_NAME} pthread) #must blow add_excutable!