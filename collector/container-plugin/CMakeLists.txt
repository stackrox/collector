add_library(collector-container-plugin SHARED ContainerPlugin.cpp)

set_target_properties(collector-container-plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "collector-container-plugin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/collector"
)

target_include_directories(collector-container-plugin PRIVATE
    ${FALCO_DIR}/userspace
    ${FALCO_DIR}/driver
)
