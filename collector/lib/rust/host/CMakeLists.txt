
add_rust_library(TARGET host
    SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    BINARY_DIRECTORY "${CMAKE_BINARY_DIR}"
    INCLUDE_DIRECTORY "${CMAKE_BINARY_DIR}/cxxbridge/"
)

add_library(host::rust_static_lib ALIAS host)
