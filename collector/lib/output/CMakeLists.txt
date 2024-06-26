file(GLOB COLLECTOR_OUTPUT_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(collector_output STATIC ${COLLECTOR_OUTPUT_SRCS})

target_link_libraries(collector_output rox-proto collector_common)
