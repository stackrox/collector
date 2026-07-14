# collector_lib: shared library linked by all three executables (collector,
# self-checks, connscrape). Centralises detection logic, gRPC transport, and
# introspection so the executables themselves are thin entry points.
file(GLOB COLLECTOR_LIB_SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/system-inspector/*.cpp
)

add_library(collector_lib ${DRIVER_HEADERS} ${COLLECTOR_LIB_SRC_FILES})
# sinsp: Falco's eBPF event engine, syscall parsing, and container-aware thread tables.
add_dependencies(collector_lib sinsp)
target_link_libraries(collector_lib sinsp)
target_link_libraries(collector_lib stdc++fs) # This is needed for GCC-8 to link against the filesystem library
# cap-ng: inspect and drop Linux capabilities for BPF operations.
target_link_libraries(collector_lib cap-ng)
target_link_libraries(collector_lib uuid)
# gRPC: bidirectional streaming to Sensor for high-volume runtime events.
target_link_libraries(collector_lib gRPC::grpc++)
# civetweb: lightweight embeddable HTTP server for introspection endpoints on 8080.
target_link_libraries(collector_lib civetweb::civetweb-cpp)
target_link_libraries(collector_lib yaml-cpp::yaml-cpp)
# Pull mode: collector hosts a /metrics endpoint on 9090 for Prometheus scraping.
target_link_libraries(collector_lib prometheus-cpp::core prometheus-cpp::pull)

# Generated protobuf/gRPC stubs for the Collector↔Sensor API.
target_link_libraries(collector_lib rox-proto)

# tcmalloc reduces malloc lock contention in the multi-threaded event pipeline.
# libprofiler enables on-demand CPU profiling in production via SIGPROF.
# Both are disabled under sanitizers/valgrind to avoid conflicts.
if(NOT DISABLE_PROFILING)
    find_library(GPERFTOOLS_PROFILER profiler)
    find_library(GPERFTOOLS_TCMALLOC tcmalloc)

    if(NOT GPERFTOOLS_PROFILER)
        message(FATAL gperftools/profiler not found)
    endif()

    if(NOT GPERFTOOLS_TCMALLOC)
        message(FATAL gperftools/tcmalloc not found)
    endif()

    target_link_libraries(collector_lib ${GPERFTOOLS_PROFILER} ${GPERFTOOLS_TCMALLOC})
endif()


if (COVERAGE)
    target_compile_options(collector_lib PUBLIC "--coverage")
    target_link_options(collector_lib PUBLIC "--coverage")
endif()

# z: gRPC compression + BPF ELF decompression. ssl/crypto: TLS to Sensor.
# bpf: libbpf for loading CO-RE eBPF programs.
target_link_libraries(collector_lib z ssl crypto bpf)
