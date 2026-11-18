cmake_minimum_required(VERSION 3.22)
project(snake_game C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(snake_game SHARED snake.cpp)

target_compile_options(snake_game PRIVATE
    -Wall -Wextra -O2
    -fvisibility=hidden
    -fvisibility-inlines-hidden)

target_link_options(snake_game PRIVATE
    -Wl,--version-script,${CMAKE_SOURCE_DIR}/export.map)

target_link_libraries(snake_game PRIVATE android log)

set_target_properties(snake_game PROPERTIES OUTPUT_NAME "snake_game")


