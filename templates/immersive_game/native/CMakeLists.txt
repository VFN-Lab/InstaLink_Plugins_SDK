cmake_minimum_required(VERSION 3.22)
project(sample_game C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(sample_game SHARED adapter.cpp)

target_compile_options(sample_game PRIVATE
    -Wall -Wextra -O2
    -fvisibility=hidden
    -fvisibility-inlines-hidden)

target_link_options(sample_game PRIVATE
    -Wl,--version-script,${CMAKE_SOURCE_DIR}/export.map)

# Link android for ANativeWindow and log for __android_log_print
target_link_libraries(sample_game PRIVATE android log)

set_target_properties(sample_game PROPERTIES OUTPUT_NAME "sample_game")


