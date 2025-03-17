
add_rust_library(TARGET rust_scraper
    SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    BINARY_DIRECTORY "${CMAKE_BINARY_DIR}"
    INCLUDE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
)

add_rust_executable(TARGET rust_scraper_exe
    SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    BINARY_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_library(rust_scraper::rust_static_lib ALIAS rust_scraper)
