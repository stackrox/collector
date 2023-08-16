project(collector-bin)

find_package(Threads)
find_package(CURL REQUIRED)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall --std=c++17 -pthread -Wno-deprecated-declarations -fno-omit-frame-pointer -rdynamic")

set(CMAKE_CXX_FLAGS_DEBUG "-g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fno-strict-aliasing -DNDEBUG")

if(NOT DEFINED ENV{DISABLE_PROFILING})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCOLLECTOR_PROFILING")
endif()

if(ADDRESS_SANITIZER)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

if(COLLECTOR_APPEND_CID)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCOLLECTOR_APPEND_CID")
endif()

if(TRACE_SINSP_EVENTS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTRACE_SINSP_EVENTS")
endif()

set(FALCO_DIR ${PROJECT_SOURCE_DIR}/../falcosecurity-libs)

add_subdirectory(${PROJECT_SOURCE_DIR}/proto)

include_directories(${PROJECT_SOURCE_DIR}/lib)
include_directories(${FALCO_DIR}/driver)
include_directories(${FALCO_DIR}/userspace)
include_directories(${FALCO_DIR}/userspace/libscap)
include_directories(${FALCO_DIR}/userspace/libsinsp)
include_directories(${FALCO_DIR}/userspace/chisel)
include_directories(${PROJECT_BINARY_DIR}/EXCLUDE_FROM_DEFAULT_BUILD/common)
include_directories(/usr/local/include)
include_directories(/usr/local/include/tbb)
include_directories(/usr/local/include/jsoncpp)
include_directories(/usr/local/include/civetweb)
include_directories(/usr/local/include/prometheus)
set(DRIVER_HEADERS ${FALCO_DIR}/driver/ppm_events_public.h ${FALCO_DIR}/driver/ppm_fillers.h)

add_definitions(-DUSE_PROTO_ARENAS)

file(GLOB COLLECTOR_LIB_SRC_FILES ${PROJECT_SOURCE_DIR}/lib/*.cpp)
add_library(collector_lib ${DRIVER_HEADERS} ${COLLECTOR_LIB_SRC_FILES})
add_dependencies(collector_lib sinsp-wrapper)
target_link_libraries(collector_lib sinsp-wrapper)
target_link_libraries(collector_lib stdc++fs) # This is needed for GCC-8 to link against the filesystem library
target_link_libraries(collector_lib cap-ng)
target_link_libraries(collector_lib uuid)
target_link_libraries(collector_lib libgrpc++.a libgrpc.a libgpr.a libupb.a libabsl_bad_optional_access.a libabsl_base.a libabsl_log_severity.a libabsl_spinlock_wait.a libabsl_str_format_internal.a libabsl_strings.a libabsl_strings_internal.a libabsl_throw_delegate.a libabsl_int128.a libabsl_raw_logging_internal.a libaddress_sorting.a)
target_link_libraries(collector_lib civetweb-cpp civetweb)

target_link_libraries(collector_lib rox-proto)

if(DEFINED ENV{WITH_RHEL_RPMS})
	target_link_libraries(collector_lib protobuf cares)
else()
	target_link_libraries(collector_lib libprotobuf.a libcares.a)
endif()

target_link_libraries(collector_lib z ssl crypto CURL::libcurl bpf)

add_executable(collector collector.cpp)
target_link_libraries(collector collector_lib)

if(NOT DEFINED ENV{DISABLE_PROFILING})
	target_link_libraries(collector profiler tcmalloc)
endif()

target_link_libraries(collector libprometheus-cpp-pull.a)
target_link_libraries(collector libprometheus-cpp-core.a)

add_executable(connscrape connscrape.cpp)
target_link_libraries(connscrape collector_lib)

add_executable(self-checks self-checks.cpp)

# Setup testing
enable_testing()

# Unit Tests
file(GLOB TEST_SRC_FILES ${PROJECT_SOURCE_DIR}/test/*.cpp)
add_executable(runUnitTests ${TEST_SRC_FILES})

if(NOT DEFINED ENV{DISABLE_PROFILING} AND (NOT DEFINED ENV{USE_VALGRIND} OR NOT "$ENV{USE_VALGRIND}"))
	target_link_libraries(runUnitTests profiler tcmalloc)
endif()

target_link_libraries(runUnitTests collector_lib)

if(DEFINED ENV{WITH_RHEL_RPMS})
	target_link_libraries(runUnitTests gtest gtest_main gmock gmock_main)
else()
	target_link_libraries(runUnitTests libgtest.a libgtest_main.a libgmock.a libgmock_main.a)
endif()

add_test(collector-tests runUnitTests)

# Falco Wrapper Library
set(BUILD_DRIVER OFF CACHE BOOL "Build the driver on Linux" FORCE)
set(USE_BUNDLED_DEPS OFF CACHE BOOL "Enable bundled dependencies instead of using the system ones" FORCE)
set(USE_BUNDLED_CARES OFF CACHE BOOL "Enable bundled dependencies instead of using the system ones" FORCE)
set(NO_LINK_GRPC_LIBS ON CACHE BOOL "Do not link gRPC libraries" FORCE)
set(WITH_CHISEL OFF CACHE BOOL "Include chisel implementation" FORCE)
set(BUILD_LIBSCAP_GVISOR OFF CACHE BOOL "Do not build gVisor support" FORCE)
set(MINIMAL_BUILD_WITH_EBPF ON CACHE BOOL "Minimal" FORCE)

# Turn OFF falco's unit tests and examples
set(CREATE_TEST_TARGETS OFF CACHE BOOL "Enable make-targets for unit testing" FORCE)
set(BUILD_LIBSCAP_EXAMPLES OFF CACHE BOOL "Build libscap examples" FORCE)
set(BUILD_LIBSINSP_EXAMPLES OFF CACHE BOOL "Build libsinsp examples" FORCE)

set(DRIVER_NAME "collector" CACHE STRING "Falco driver name" FORCE)

file(STRINGS "${PROJECT_SOURCE_DIR}/../kernel-modules/MODULE_VERSION" DRIVER_VERSION)

set(DRIVER_VERSION "${DRIVER_VERSION}" CACHE STRING "Falco driver version" FORCE)
set(DRIVER_DEVICE_NAME "${DRIVER_NAME}" CACHE STRING "Driver device name" FORCE)

set(SCAP_BPF_PROGS_MAX "256" CACHE STRING "Number of BPF progams supported in Falco" FORCE)
set(SCAP_HOST_ROOT_ENV_VAR_NAME "COLLECTOR_HOST_ROOT" CACHE STRING "Host root environment variable name" FORCE)

# ppc64le is not supported for the "modern" probe
if(NOT CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "ppc64le")
	set(BUILD_LIBSCAP_MODERN_BPF ON CACHE BOOL "Enable modern bpf engine" FORCE)
endif()

set(MODERN_BPF_EXCLUDE_PROGS "^(openat2|ppoll|setsockopt|clone3|io_uring_setup|nanosleep)$" CACHE STRING "Set of syscalls to exclude from modern bpf engine " FORCE)

add_subdirectory(${FALCO_DIR} EXCLUDE_FROM_DEFAULT_BUILD)
