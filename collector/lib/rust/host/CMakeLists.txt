set(HOST_CXX_FILE "${CMAKE_BINARY_DIR}/cxxbridge/host/src/lib.rs.cc")

add_rust_library(TARGET host
    SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    BINARY_DIRECTORY "${CMAKE_BINARY_DIR}"
    INCLUDE_DIRECTORY "${CMAKE_BINARY_DIR}/cxxbridge/"
    BYPRODUCTS "${HOST_CXX_FILE}"
)

add_library(host_cxx "${HOST_CXX_FILE}")
add_dependencies(host_cxx host)

add_library(host::rust_static_lib ALIAS host)
