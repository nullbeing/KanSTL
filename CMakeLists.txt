cmake_minimum_required(VERSION 3.13)
project(KanSTL LANGUAGES CXX)

find_package(ALSA REQUIRED)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(kanstl
    src/main.cpp
    src/canvas.cpp
    src/file_browser.cpp
    src/font.cpp
    src/framebuffer.cpp
    src/keyboard.cpp
    src/png.cpp
    src/renderer.cpp
    src/sound.cpp
    src/stl_loader.cpp
    src/ui.cpp
)

target_link_libraries(kanstl PRIVATE ALSA::ALSA Threads::Threads)

target_include_directories(kanstl PRIVATE include)
target_compile_options(kanstl PRIVATE -Wall -Wextra)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    target_compile_options(kanstl PRIVATE -O3 -march=armv8-a -mtune=cortex-a53)
endif()
