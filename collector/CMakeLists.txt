# Build configuration for the three collector executables:
#
#   collector   - Main daemon. Runs as PID 1 in the DaemonSet pod, attaches
#                 the CO-RE eBPF probe via Falco's sinsp library, and streams
#                 events to Sensor over gRPC.
#
#   self-checks - Lightweight startup validator. Confirms the BPF CO-RE driver
#                 can load on the current kernel before the full collector
#                 starts, enabling fast-fail with a clear diagnostic.
#
#   connscrape  - Diagnostic utility for dumping live connection state from
#                 /proc. Useful for debugging network-related collection issues.
#
project(collector-bin)

find_package(Threads)
find_package(CURL REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(civetweb CONFIG REQUIRED)
find_package(prometheus-cpp CONFIG REQUIRED)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})
# -fno-omit-frame-pointer: needed for gperftools CPU profiling and perf.
# -rdynamic: crash-handler backtraces need exported symbols for readable names.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall --std=c++17 -pthread -Wno-deprecated-declarations -fno-omit-frame-pointer -rdynamic")

set(CMAKE_CXX_FLAGS_DEBUG "-g -ggdb -D_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fno-strict-aliasing -DNDEBUG")

# ASan/TSan and gperftools both intercept malloc, so they conflict at link time.
if(ADDRESS_SANITIZER)
	set(DISABLE_PROFILING ON)
	set(USE_VALGRIND OFF)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=undefined -DGRPC_ASAN_SUPPRESSED")
endif()

if(THREAD_SANITIZER)
	set(DISABLE_PROFILING ON)
	set(USE_VALGRIND OFF)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -DGRPC_TSAN_SUPPRESSED")
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

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/lib/CollectorVersion.h.in ${CMAKE_CURRENT_BINARY_DIR}/CollectorVersion.h)

set(FALCO_DIR ${PROJECT_SOURCE_DIR}/../falcosecurity-libs)

add_subdirectory(${PROJECT_SOURCE_DIR}/proto)

include_directories(${PROJECT_SOURCE_DIR}/lib)
include_directories(${FALCO_DIR}/driver)
include_directories(${FALCO_DIR}/userspace)
include_directories(${FALCO_DIR}/userspace/libscap)
include_directories(${FALCO_DIR}/userspace/libsinsp)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
set(DRIVER_HEADERS ${FALCO_DIR}/driver/ppm_events_public.h ${FALCO_DIR}/driver/ppm_fillers.h)

# Enables protobuf arena allocation — batches allocations and frees them all at
# once, reducing heap fragmentation in the high-throughput gRPC event path.
add_definitions(-DUSE_PROTO_ARENAS)

# Redirect sinsp ASSERT macros to log warnings instead of abort(). A crash
# in the collector process would blind the entire node.
add_definitions(-DASSERT_TO_LOG)

# Only track socket file descriptors — ignoring file/pipe FDs avoids tracking
# millions of irrelevant descriptors and substantially reduces memory usage.
add_definitions(-DSCAP_SOCKET_ONLY_FD)

# Limit cgroup parsing to these subsystems to avoid scanning dozens of
# irrelevant controllers per process when resolving container IDs.
add_definitions("-DINTERESTING_SUBSYS=\"perf_event\", \"cpu\", \"cpuset\", \"memory\"")

add_subdirectory(lib)

add_executable(collector collector.cpp)
target_link_libraries(collector collector_lib)

add_executable(connscrape connscrape.cpp)
target_link_libraries(connscrape collector_lib)

add_executable(self-checks self-checks.cpp)

add_subdirectory(test)

# Falco sinsp library configuration.
# Legacy kernel module driver is unused — collector uses CO-RE eBPF (below).
set(BUILD_DRIVER OFF CACHE BOOL "Build the driver on Linux" FORCE)
set(USE_BUNDLED_DEPS OFF CACHE BOOL "Enable bundled dependencies instead of using the system ones" FORCE)
set(USE_BUNDLED_CARES OFF CACHE BOOL "Enable bundled dependencies instead of using the system ones" FORCE)
# gVisor intercepts syscalls differently and is not a supported runtime.
set(BUILD_LIBSCAP_GVISOR OFF CACHE BOOL "Do not build gVisor support" FORCE)
set(MINIMAL_BUILD OFF CACHE BOOL "Minimal" FORCE)
# Reduces per-thread memory from ~1KB to a few hundred bytes. Prevents OOM
# on nodes with thousands of threads.
set(SINSP_SLIM_THREADINFO ON CACHE BOOL "Slim threadinfo" FORCE)
# Project libraries default to static linkage for the minimal UBI-micro
# container. Required shared runtime deps (e.g. libelf) remain dynamic.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build position independent libraries and executables" FORCE)
# Exception to static linking: libbpf dlopen()s libelf at runtime to parse
# BPF ELF objects, so libelf must remain a shared library.
set(LIBELF_LIB_SUFFIX ".so" CACHE STRING "Use libelf.so" FORCE)

# Turn OFF falco's unit tests and examples
set(CREATE_TEST_TARGETS OFF CACHE BOOL "Enable make-targets for unit testing" FORCE)
set(BUILD_LIBSCAP_EXAMPLES OFF CACHE BOOL "Build libscap examples" FORCE)
set(BUILD_LIBSINSP_EXAMPLES OFF CACHE BOOL "Build libsinsp examples" FORCE)

# Collector runs in a container but needs the host's /proc and /sys. The
# DaemonSet mounts the host root at /host; this tells sinsp to prefix all
# host filesystem lookups accordingly (e.g. /host/proc instead of /proc).
set(SCAP_HOST_ROOT_ENV_VAR_NAME "COLLECTOR_HOST_ROOT" CACHE STRING "Host root environment variable name" FORCE)

# CO-RE (Compile Once - Run Everywhere) eBPF eliminates the need to ship
# per-kernel driver modules, simplifying deployment across kernel versions.
set(BUILD_LIBSCAP_MODERN_BPF ON CACHE BOOL "Enable modern bpf engine" FORCE)
set(MODERN_BPF_DEBUG_MODE ${BPF_DEBUG_MODE} CACHE BOOL "Enable BPF debug prints" FORCE)

# Exclude high-frequency syscalls (ppoll, nanosleep) and ones not used for
# detection (openat2, setsockopt, io_uring_setup) to reduce BPF memory
# footprint and verifier load time.
set(MODERN_BPF_EXCLUDE_PROGS "^(openat2|ppoll|setsockopt|io_uring_setup|nanosleep)$" CACHE STRING "Set of syscalls to exclude from modern bpf engine " FORCE)

add_subdirectory(${FALCO_DIR} falco)
