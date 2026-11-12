cmake_minimum_required(VERSION 3.13)
project(stlviewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(stlviewer
    src/main.cpp
    src/canvas.cpp
    src/font.cpp
    src/framebuffer.cpp
    src/keyboard.cpp
    src/png.cpp
    src/renderer.cpp
    src/stl_loader.cpp
    src/ui.cpp
)

target_include_directories(stlviewer PRIVATE include)
target_compile_options(stlviewer PRIVATE -Wall -Wextra)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    target_compile_options(stlviewer PRIVATE -O3 -march=armv8-a -mtune=cortex-a53)
endif()
