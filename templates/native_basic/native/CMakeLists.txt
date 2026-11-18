cmake_minimum_required(VERSION 3.22)
project(native_basic_adapter C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(native_basic_adapter SHARED adapter.cpp)

target_compile_options(native_basic_adapter PRIVATE
    -Wall -Wextra -O2
    -fvisibility=hidden
    -fvisibility-inlines-hidden)

target_link_options(native_basic_adapter PRIVATE
    -Wl,--version-script,${CMAKE_SOURCE_DIR}/export.map)

target_link_libraries(native_basic_adapter PRIVATE log)

set_target_properties(native_basic_adapter PROPERTIES OUTPUT_NAME "native_basic_adapter")
