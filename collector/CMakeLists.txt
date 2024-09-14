project(collector-bin)

find_package(Threads)
find_package(CURL REQUIRED)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall --std=c++17 -pthread -Wno-deprecated-declarations -fno-omit-frame-pointer -rdynamic")

set(CMAKE_CXX_FLAGS_DEBUG "-g -ggdb -D_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fno-strict-aliasing -DNDEBUG")

if(ADDRESS_SANITIZER)
	set(DISABLE_PROFILING ON)
	set(USE_VALGRIND OFF)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=undefined -DGRPC_ASAN_SUPRESSED")
endif()

if(USE_VALGRIND)
	set(DISABLE_PROFILING ON)
endif()

if(NOT DISABLE_PROFILING)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCOLLECTOR_PROFILING")
endif()

if(TRACE_SINSP_EVENTS)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTRACE_SINSP_EVENTS")
endif()

if(NOT BPF_DEBUG_MODE)
	set(BPF_DEBUG_MODE OFF)
endif()

if (NOT COLLECTOR_VERSION)
	set(COLLECTOR_VERSION "0.0.0")
endif()

if(USE_CCACHE)
    set(CMAKE_C_COMPILER_LAUNCHER ccache)
    set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/lib/CollectorVersion.h.in ${CMAKE_CURRENT_BINARY_DIR}/CollectorVersion.h)

set(FALCO_DIR ${PROJECT_SOURCE_DIR}/../falcosecurity-libs)

add_subdirectory(${PROJECT_SOURCE_DIR}/proto)

include_directories(${PROJECT_SOURCE_DIR}/lib)
include_directories(${FALCO_DIR}/driver)
include_directories(${FALCO_DIR}/userspace)
include_directories(${FALCO_DIR}/userspace/libscap)
include_directories(${FALCO_DIR}/userspace/libsinsp)
include_directories(${FALCO_DIR}/userspace/chisel)
include_directories(${FALCO_DIR}/userspace/common)
include_directories(${PROJECT_BINARY_DIR}/EXCLUDE_FROM_DEFAULT_BUILD/common)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(/usr/local/include)
include_directories(/usr/local/include/tbb)
include_directories(/usr/local/include/jsoncpp)
include_directories(/usr/local/include/civetweb)
include_directories(/usr/local/include/prometheus)
set(DRIVER_HEADERS ${FALCO_DIR}/driver/ppm_events_public.h ${FALCO_DIR}/driver/ppm_fillers.h)

add_definitions(-DUSE_PROTO_ARENAS)

add_definitions(-DASSERT_TO_LOG)

# Optimization, only process socket file descriptors
add_definitions(-DSCAP_SOCKET_ONLY_FD)

# Filter out some cgroup subsys
add_definitions("-DINTERESTING_SUBSYS=\"perf_event\", \"cpu\", \"cpuset\", \"memory\"")

add_definitions(-DASSERT_TO_LOG)

add_subdirectory(lib)

add_executable(collector collector.cpp)
target_link_libraries(collector collector_lib)

target_link_libraries(collector libprometheus-cpp-pull.a)
target_link_libraries(collector libprometheus-cpp-core.a)

add_executable(connscrape connscrape.cpp)
target_link_libraries(connscrape collector_lib)

add_executable(self-checks self-checks.cpp)

add_subdirectory(test)

# Falco Wrapper Library
if(USE_CCACHE)
  set(MODERN_CLANG_EXE "/usr/local/bin/ccache-clang" CACHE STRING "USE CCACHE" FORCE)
endif()
set(BUILD_DRIVER OFF CACHE BOOL "Build the driver on Linux" FORCE)
set(USE_BUNDLED_DEPS OFF CACHE BOOL "Enable bundled dependencies instead of using the system ones" FORCE)
set(USE_BUNDLED_CARES OFF CACHE BOOL "Enable bundled dependencies instead of using the system ones" FORCE)
set(BUILD_LIBSCAP_GVISOR OFF CACHE BOOL "Do not build gVisor support" FORCE)
set(MINIMAL_BUILD OFF CACHE BOOL "Minimal" FORCE)
set(SINSP_SLIM_THREADINFO ON CACHE BOOL "Slim threadinfo" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build position independent libraries and executables" FORCE)
set(LIBELF_LIB_SUFFIX ".so" CACHE STRING "Use libelf.so" FORCE)

# Turn OFF falco's unit tests and examples
set(CREATE_TEST_TARGETS OFF CACHE BOOL "Enable make-targets for unit testing" FORCE)
set(BUILD_LIBSCAP_EXAMPLES OFF CACHE BOOL "Build libscap examples" FORCE)
set(BUILD_LIBSINSP_EXAMPLES OFF CACHE BOOL "Build libsinsp examples" FORCE)

set(SCAP_HOST_ROOT_ENV_VAR_NAME "COLLECTOR_HOST_ROOT" CACHE STRING "Host root environment variable name" FORCE)

set(BUILD_LIBSCAP_MODERN_BPF ON CACHE BOOL "Enable modern bpf engine" FORCE)
set(MODERN_BPF_DEBUG_MODE ${BPF_DEBUG_MODE} CACHE BOOL "Enable BPF debug prints" FORCE)

set(MODERN_BPF_EXCLUDE_PROGS "^(openat2|ppoll|setsockopt|clone3|io_uring_setup|nanosleep)$" CACHE STRING "Set of syscalls to exclude from modern bpf engine " FORCE)

add_subdirectory(${FALCO_DIR} falco)
