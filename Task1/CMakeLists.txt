cmake_minimum_required(VERSION 3.15)
project(detect LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Release/Debug" FORCE)
endif()

find_package(Threads REQUIRED)
find_package(OpenMP REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(ZLIB REQUIRED)

add_library(image_io_deps INTERFACE)
target_link_libraries(image_io_deps INTERFACE PNG::PNG JPEG::JPEG ZLIB::ZLIB)

add_subdirectory(src)
