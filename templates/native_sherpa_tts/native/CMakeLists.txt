cmake_minimum_required(VERSION 3.22)
project(sherpa_tts_adapter C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

set(SHERPA_LIB_DIR "${CMAKE_SOURCE_DIR}/../runtime_bundle/jniLibs/${ANDROID_ABI}")

add_library(sherpa-onnx-c-api SHARED IMPORTED)
set_target_properties(sherpa-onnx-c-api PROPERTIES
    IMPORTED_LOCATION "${SHERPA_LIB_DIR}/libsherpa-onnx-c-api.so")

add_library(sherpa-onnx-cxx-api SHARED IMPORTED)
set_target_properties(sherpa-onnx-cxx-api PROPERTIES
    IMPORTED_LOCATION "${SHERPA_LIB_DIR}/libsherpa-onnx-cxx-api.so")

add_library(onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime PROPERTIES
    IMPORTED_LOCATION "${SHERPA_LIB_DIR}/libonnxruntime.so")

add_library(sherpa_tts_adapter SHARED adapter.cpp)

target_compile_options(sherpa_tts_adapter PRIVATE
    -Wall -Wextra -O2
    -fvisibility=hidden
    -fvisibility-inlines-hidden)

target_link_options(sherpa_tts_adapter PRIVATE
    -Wl,--version-script,${CMAKE_SOURCE_DIR}/export.map)

target_link_libraries(sherpa_tts_adapter PRIVATE
    sherpa-onnx-c-api
    sherpa-onnx-cxx-api
    onnxruntime
    android
    log
    atomic)

set_target_properties(sherpa_tts_adapter PROPERTIES OUTPUT_NAME "sherpa_tts_adapter")
