add_library(verified STATIC
    extracted/NetworkTypes.c
    extracted/NetworkOps.c
    extracted/NRadixPulse.c
)
target_include_directories(verified PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(verified PUBLIC
    -include ${CMAKE_CURRENT_SOURCE_DIR}/extracted/krml_compat.h)
