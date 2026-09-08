# Reuse the pinned upstream fixture without enabling its entire test suite.
find_package(GTest CONFIG REQUIRED)
set(FALCO_TEST_DIR "${FALCO_DIR}/userspace/libsinsp/test")
configure_file("${FALCO_TEST_DIR}/libsinsp_test_var.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/libsinsp_test_var.h")
file(GLOB FALCO_TEST_HELPERS "${FALCO_TEST_DIR}/helpers/*.cpp")
add_library(collector_replay_support STATIC
    "${FALCO_TEST_DIR}/sinsp_with_test_input.cpp"
    "${FALCO_TEST_DIR}/test_utils.cpp"
    ${FALCO_TEST_HELPERS})
target_include_directories(collector_replay_support PUBLIC
    "${FALCO_DIR}" "${FALCO_TEST_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(collector_replay_support PUBLIC sinsp GTest::gtest)

add_executable(ContainerPluginReplayTest ContainerPluginReplayTest.cpp)
target_link_libraries(ContainerPluginReplayTest PRIVATE
    collector_replay_support collector_lib GTest::gtest_main)
add_dependencies(ContainerPluginReplayTest collector-container-plugin)
add_test(NAME ContainerPluginReplayTest COMMAND ContainerPluginReplayTest)
set_tests_properties(ContainerPluginReplayTest PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "ROX_COLLECTOR_CONTAINER_PLUGIN_PATH=$<TARGET_FILE:collector-container-plugin>")
