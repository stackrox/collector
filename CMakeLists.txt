# Root CMake configuration for collector.
#
# The actual build logic lives in collector/CMakeLists.txt. This file exists
# so CMake can be invoked from the repo root (standard convention) and to
# hold project-wide settings like enable_testing().
cmake_minimum_required(VERSION 3.15)
project(collector)

enable_testing()

add_subdirectory(collector)
