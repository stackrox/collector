
file(GLOB files "*.cpp")

foreach (file ${files})
    get_filename_component(target "${file}" NAME_WE)

    add_executable(${target} ${file})
    target_link_libraries(${target} collector_lib)
    target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:collector_lib,INCLUDE_DIRECTORIES>)
endforeach()
