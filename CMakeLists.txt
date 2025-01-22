# Minimum required version of CMake
cmake_minimum_required(VERSION 3.10)

# Project name and version
project(CoralCamera VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(src)

# Source files
set(SOURCES
    src/camerastreamer.cc
    src/inferencewrapper.cc
    src/main.cc
)

# Header files (optional, for IDEs or organization)
set(HEADERS
    src/camerastreamer.h
    src/inferencewrapper.h
)

# Find GStreamer using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_BASE REQUIRED gstreamer-plugins-base-1.0)

# Include GStreamer directories
include_directories(${GSTREAMER_INCLUDE_DIRS} ${GSTREAMER_BASE_INCLUDE_DIRS})
link_directories(${GSTREAMER_LIBRARY_DIRS} ${GSTREAMER_BASE_LIBRARY_DIRS})

# Add GStreamer compile definitions
add_definitions(${GSTREAMER_CFLAGS_OTHER})

# Add executable
add_executable(coral-camera ${SOURCES} ${HEADERS})

# Link GStreamer libraries
target_link_libraries(coral-camera ${GSTREAMER_LIBRARIES} ${GSTREAMER_BASE_LIBRARIES})
