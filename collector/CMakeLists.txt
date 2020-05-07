cmake_minimum_required(VERSION 2.8)
project (collector)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall --std=c++11 -pthread -Wno-deprecated-declarations")

set(CMAKE_CXX_FLAGS_DEBUG "-g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fno-strict-aliasing -DNDEBUG")

if(ADDRESS_SANITIZER)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
endif()

link_directories(/usr/local/lib)

include_directories(${PROJECT_SOURCE_DIR}/lib)
include_directories(${PROJECT_SOURCE_DIR}/generated/proto)
include_directories(${PROJECT_SOURCE_DIR}/generated/rox-proto)
include_directories(${PROJECT_SOURCE_DIR}/generated/googleapis)
include_directories(/usr/local/include)
include_directories(/usr/local/sysdig/userspace)
include_directories(/usr/local/sysdig/userspace/libscap)
include_directories(/usr/local/sysdig/userspace/libsinsp)
include_directories(/usr/local/include/grpc)
include_directories(/usr/local/include/tbb)
include_directories(/usr/local/include/jsoncpp)
include_directories(/usr/include/civetweb)
include_directories(/usr/include/prometheus)
include_directories(/usr/include/google/protobuf)

add_definitions(-DUSE_PROTO_ARENAS)

file(GLOB COLLECTOR_LIB_SRC_FILES ${PROJECT_SOURCE_DIR}/lib/*.cpp)
file(GLOB_RECURSE PROTO_SRC_FILES ${PROJECT_SOURCE_DIR}/generated/*.cc)
add_library(collector_lib ${COLLECTOR_LIB_SRC_FILES} ${PROTO_SRC_FILES})
target_link_libraries(collector_lib sinsp-wrapper)
target_link_libraries(collector_lib cap-ng)
target_link_libraries(collector_lib uuid)
target_link_libraries(collector_lib libgrpc++.a libgrpc.a libgpr.a libprotobuf.a z ssl crypto)

add_executable(collector collector.cpp)
target_link_libraries(collector collector_lib)
target_link_libraries(collector libprometheus-cpp-pull.a)
target_link_libraries(collector libprometheus-cpp-core.a)

add_executable(connscrape connscrape.cpp)
target_link_libraries(connscrape collector_lib)

# Setup testing
enable_testing()

# Unit Tests
file(GLOB TEST_SRC_FILES ${PROJECT_SOURCE_DIR}/test/*.cpp)
add_executable(runUnitTests ${TEST_SRC_FILES})
target_link_libraries(runUnitTests collector_lib)
target_link_libraries(runUnitTests libgtest.a)
target_link_libraries(runUnitTests libgtest_main.a)
target_link_libraries(runUnitTests libgmock.a)
target_link_libraries(runUnitTests libgmock_main.a)

add_test(collector-tests runUnitTests)
