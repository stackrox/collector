cmake_minimum_required(VERSION 2.8)
project (collector)

# set(CMAKE_CXX_STANDARD 11)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -std=c++11 -pthread -Wno-deprecated-declarations")

set(CMAKE_CXX_FLAGS_DEBUG "-g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fno-strict-aliasing -DNDEBUG")

if(ADDRESS_SANITIZER)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
endif()

add_subdirectory(abseil-cpp)

include_directories(${PROJECT_SOURCE_DIR}/lib)
include_directories(${PROJECT_SOURCE_DIR}/generated/proto)
include_directories(${PROJECT_SOURCE_DIR}/generated/rox-proto)
include_directories(${PROJECT_SOURCE_DIR}/generated/googleapis)
include_directories(${PROJECT_SOURCE_DIR}/sysdig/src/driver)
include_directories(${PROJECT_SOURCE_DIR}/sysdig/src/userspace)
include_directories(${PROJECT_SOURCE_DIR}/sysdig/src/userspace/libscap)
include_directories(${PROJECT_SOURCE_DIR}/sysdig/src/userspace/libsinsp)
include_directories(/usr/local/include)
include_directories(/usr/local/include/grpc)
include_directories(/usr/local/include/tbb)
include_directories(/usr/local/include/jsoncpp)
include_directories(/usr/include/civetweb)
include_directories(/usr/include/prometheus)
include_directories(/usr/include/google/protobuf)
set(SYSDIG_DRIVER_HEADERS ${PROJECT_SOURCE_DIR}/sysdig/src/driver/ppm_events_public.h ${PROJECT_SOURCE_DIR}/sysdig/src/driver/ppm_fillers.h)

add_definitions(-DUSE_PROTO_ARENAS)

file(GLOB COLLECTOR_LIB_SRC_FILES ${PROJECT_SOURCE_DIR}/lib/*.cpp)
file(GLOB_RECURSE PROTO_SRC_FILES ${PROJECT_SOURCE_DIR}/generated/*.cc)
add_library(collector_lib ${SYSDIG_DRIVER_HEADERS} ${COLLECTOR_LIB_SRC_FILES} ${PROTO_SRC_FILES})
add_dependencies(collector_lib sinsp-wrapper)
target_link_libraries(collector_lib sinsp-wrapper)
target_link_libraries(collector_lib cap-ng)
target_link_libraries(collector_lib uuid)
target_link_libraries(collector_lib absl::strings)
target_link_libraries(collector_lib libgrpc++.a libgrpc.a libgpr.a libprotobuf.a libupb.a libabsl_bad_optional_access.a libabsl_base.a libabsl_dynamic_annotations.a libabsl_log_severity.a libabsl_spinlock_wait.a libabsl_str_format_internal.a libabsl_strings.a libabsl_strings_internal.a libabsl_throw_delegate.a libabsl_int128.a libabsl_raw_logging_internal.a libaddress_sorting.a libcares.a z ssl crypto)

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
target_link_libraries(runUnitTests libgmock.a)
target_link_libraries(runUnitTests absl::flags_parse)

add_test(CollectorTests runUnitTests --testdata_dir "${PROJECT_SOURCE_DIR}/test/testdata")

# Sysdig Wrapper Library
set(BUILD_DRIVER OFF CACHE BOOL "Build the driver on Linux" FORCE)
set(USE_BUNDLED_DEPS OFF CACHE BOOL "Enable bundled dependencies instead of using the system ones" FORCE)
set(NO_LINK_GRPC_LIBS ON CACHE BOOL "Do not link gRPC libraries" FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/sysdig/src EXCLUDE_FROM_DEFAULT_BUILD)
