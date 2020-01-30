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
set(LIBS ${PROJECT_NAME} pthread yaml-cpp)


set(LIB_SRC
        log.cc
        config.cc
        thread.cc
        fiber.cc
        scheduler.cc
        iomanager.cc
        timer.cc
        address.cc
        util.cc
        )

add_library(${PROJECT_NAME} SHARED ${LIB_SRC})

add_executable(log_test tests/log_test.cc)
add_dependencies(log_test ${PROJECT_NAME})
target_link_libraries(log_test ${LIBS})

add_executable(config_test tests/config_test.cc)
add_dependencies(config_test ${PROJECT_NAME})
target_link_libraries(config_test ${PROJECT_NAME} ${YAMLCPP} ${THREAD})

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

#valgrind --tool=memcheck --track-origins=yes --leak-check=full --show-leak-kinds=all ./scheduler_test
