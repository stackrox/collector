file(GLOB COLLECTOR_SOURCES_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/falco/*.cpp)

add_library(collector_sources STATIC ${COLLECTOR_SOURCES_SRC})
target_include_directories(collector_sources PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(collector_sources rox-proto)
