include(FetchContent)
FetchContent_Declare(rapidcheck
  GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
  GIT_TAG c60b8ec)
FetchContent_MakeAvailable(rapidcheck)

add_executable(test_verified_network test_network.cpp)
target_link_libraries(test_verified_network collector_lib verified rapidcheck)
add_test(NAME test_verified_network COMMAND test_verified_network)

add_executable(test_verified_nradix test_nradix.cpp)
target_link_libraries(test_verified_nradix collector_lib verified rapidcheck)
add_test(NAME test_verified_nradix COMMAND test_verified_nradix)
