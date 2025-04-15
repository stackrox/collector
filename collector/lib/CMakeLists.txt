add_subdirectory(rust)

file(GLOB COLLECTOR_LIB_SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/system-inspector/*.cpp
)

add_library(collector_lib ${DRIVER_HEADERS} ${COLLECTOR_LIB_SRC_FILES})
add_dependencies(collector_lib sinsp)
target_link_libraries(collector_lib sinsp)
target_link_libraries(collector_lib stdc++fs) # This is needed for GCC-8 to link against the filesystem library
target_link_libraries(collector_lib cap-ng)
target_link_libraries(collector_lib uuid)
target_link_libraries(collector_lib gRPC::grpc++)
target_link_libraries(collector_lib civetweb::civetweb-cpp)
target_link_libraries(collector_lib yaml-cpp::yaml-cpp)

target_link_libraries(collector_lib rust_test::rust_static_lib)
target_link_libraries(collector_lib host_cxx)
target_link_libraries(collector_lib host::rust_static_lib)

target_link_libraries(collector_lib rox-proto)

target_include_directories(collector_lib INTERFACE $<TARGET_PROPERTY:rust_test::rust_static_lib,INTERFACE_INCLUDE_DIRECTORIES>)

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

target_link_libraries(collector_lib z ssl crypto bpf)
