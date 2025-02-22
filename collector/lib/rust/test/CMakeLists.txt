
add_rust_library(TARGET rust_test
    SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    BINARY_DIRECTORY "${CMAKE_BINARY_DIR}"
    INCLUDE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
)

add_library(rust_test::rust_static_lib ALIAS rust_test)
