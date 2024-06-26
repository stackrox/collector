file(GLOB COLLECTOR_STATS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(collector_stats STATIC "${COLLECTOR_STATS_SRCS}")

target_link_libraries(collector_stats collector_common)
