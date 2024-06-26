
file(GLOB COLLECTOR_SYS_INSPECTOR_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(collector_system_inspector STATIC "${COLLECTOR_SYS_INSPECTOR_SRCS}")

target_link_libraries(collector_system_inspector sinsp rox-proto collector_common)
