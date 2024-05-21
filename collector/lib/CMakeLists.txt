add_subdirectory(sources)

file(GLOB COLLECTOR_LIB_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/system-inspector/*.cpp)

add_library(collector_lib ${DRIVER_HEADERS} ${COLLECTOR_LIB_SRC_FILES})
add_dependencies(collector_lib sinsp)
target_link_libraries(collector_lib sinsp)
target_link_libraries(collector_lib stdc++fs) # This is needed for GCC-8 to link
                                              # against the filesystem library
target_link_libraries(collector_lib cap-ng)
target_link_libraries(collector_lib uuid)
target_link_libraries(
  collector_lib
  libgrpc++.a
  libgrpc.a
  libgpr.a
  libupb.a
  libabsl_bad_optional_access.a
  libabsl_base.a
  libabsl_log_severity.a
  libabsl_spinlock_wait.a
  libabsl_str_format_internal.a
  libabsl_strings.a
  libabsl_strings_internal.a
  libabsl_throw_delegate.a
  libabsl_int128.a
  libabsl_raw_logging_internal.a
  libaddress_sorting.a)
target_link_libraries(collector_lib civetweb-cpp civetweb)

target_link_libraries(collector_lib rox-proto)

if(DEFINED ENV{WITH_RHEL_RPMS})
  target_link_libraries(collector_lib protobuf cares)
else()
  target_link_libraries(collector_lib libprotobuf.a libcares.a)
endif()

if(NOT DISABLE_PROFILING)
  target_link_libraries(collector_lib profiler tcmalloc)
endif()

target_link_libraries(collector_lib z ssl crypto CURL::libcurl bpf)
