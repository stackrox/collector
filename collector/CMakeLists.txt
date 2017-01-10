cmake_minimum_required(VERSION 2.8)
project (collector)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})
set(CMAKE_CXX_FLAGS "-g -Wall -pthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -ggdb --std=c++0x")

if(ADDRESS_SANITIZER)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
endif()

link_directories(/usr/local/lib)

include_directories(${PROJECT_SOURCE_DIR}/lib)
include_directories(${PROJECT_SOURCE_DIR}/container/include)
include_directories(${PROJECT_SOURCE_DIR}/../ext/sysdig/src/userspace/sysdig)
include_directories(/usr/local/include)
include_directories(/usr/include/librdkafka)
include_directories(/usr/include/jsoncpp)

file(GLOB COLLECTOR_LIB_SRC_FILES ${PROJECT_SOURCE_DIR}/lib/*.cpp)
add_library(collector_lib ${COLLECTOR_LIB_SRC_FILES})

add_executable(collector collector.cpp)
target_link_libraries(collector collector_lib)
target_link_libraries(collector boost_regex)
target_link_libraries(collector curl)
target_link_libraries(collector jsoncpp)
target_link_libraries(collector microhttpd)
target_link_libraries(collector rdkafka)
target_link_libraries(collector sysdignative)

# Setup testing
enable_testing()

# Unit Tests
file(GLOB TEST_SRC_FILES ${PROJECT_SOURCE_DIR}/test/*.cpp)
add_executable(runUnitTests ${TEST_SRC_FILES})
target_link_libraries(runUnitTests collector_lib)
target_link_libraries(runUnitTests boost_regex)
target_link_libraries(runUnitTests curl)
target_link_libraries(runUnitTests jsoncpp)
target_link_libraries(runUnitTests microhttpd)
target_link_libraries(runUnitTests rdkafka)
target_link_libraries(runUnitTests libgtest.a)
target_link_libraries(runUnitTests libgtest_main.a)
target_link_libraries(runUnitTests libgmock.a)
target_link_libraries(runUnitTests libgmock_main.a)
target_link_libraries(runUnitTests sysdignative)
add_test(collector-tests runUnitTests)

