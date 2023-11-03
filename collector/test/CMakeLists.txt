# Setup testing
enable_testing()

# Unit Tests
file(GLOB TEST_SRC_FILES ${PROJECT_SOURCE_DIR}/test/*.cpp)
foreach(test_file ${TEST_SRC_FILES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable("${test_name}" "${test_file}")

    if(NOT DISABLE_PROFILING AND NOT USE_VALGRIND)
        target_link_libraries(${test_name} profiler tcmalloc)
    endif()

    target_link_libraries(${test_name} collector_lib)

    if(DEFINED ENV{WITH_RHEL_RPMS})
        target_link_libraries(${test_name} gtest gtest_main gmock gmock_main)
    else()
        target_link_libraries(${test_name} libgtest.a libgtest_main.a libgmock.a libgmock_main.a)
    endif()

    add_test(${test_name} ${test_name})

    if(USE_VALGRIND)
        add_test(NAME memcheck_${test_name} COMMAND valgrind --leak-check=full $<TARGET_FILE:${test_name}>)
    endif()
endforeach()
