# Setup testing

find_package(GTest CONFIG REQUIRED)
enable_testing()

# Unit Tests
file(GLOB TEST_SRC_FILES ${PROJECT_SOURCE_DIR}/test/*.cpp)
foreach(test_file ${TEST_SRC_FILES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable("${test_name}" "${test_file}")

    target_link_libraries(${test_name} collector_lib)
    target_link_libraries(${test_name} GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)

    add_test(${test_name} ${test_name})

    if(USE_VALGRIND)
        # TODO: This test has a deadlock when running on valgrind. Further investigation needed.
        string(COMPARE EQUAL ${test_name} NetworkStatusNotifierTest res)
        if(res)
            continue()
        endif()

        add_test(NAME memcheck_${test_name} COMMAND valgrind -q --leak-check=full --trace-children=yes $<TARGET_FILE:${test_name}>)
    endif()
endforeach()
