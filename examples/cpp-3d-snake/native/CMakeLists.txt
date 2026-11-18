cmake_minimum_required(VERSION 3.22.1)
project(snake3d)

# Set the library name
set(LIB_NAME snake3d)

# Add source files
add_library(${LIB_NAME} SHARED
    snake3d.cpp
)

# Link required Android libraries (log and native_window)
target_link_libraries(${LIB_NAME}
    android
    log
)

# Strip symbols to reduce size
set_target_properties(${LIB_NAME} PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN 1
)

# Use our export map
set_target_properties(${LIB_NAME} PROPERTIES LINK_FLAGS "-Wl,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/export.map\"")
