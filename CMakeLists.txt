cmake_minimum_required(VERSION 3.15)
project(collector)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

enable_testing()

add_subdirectory(collector)
