cmake_minimum_required(VERSION 3.22)
project(dodger3d C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(dodger3d SHARED game3d.cpp)

target_compile_options(dodger3d PRIVATE
    -Wall -Wextra -O2
    -fvisibility=hidden
    -fvisibility-inlines-hidden)

target_link_options(dodger3d PRIVATE
    -Wl,--version-script,${CMAKE_SOURCE_DIR}/export.map)

target_link_libraries(dodger3d PRIVATE android log)

set_target_properties(dodger3d PROPERTIES OUTPUT_NAME "dodger3d")


