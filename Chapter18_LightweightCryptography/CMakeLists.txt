cmake_minimum_required(VERSION 3.16)

set(CMAKE_INSTALL_PREFIX "$ENV{HOME}" CACHE PATH
    "Install Chapter 18 labs under the current user's home directory"
    FORCE
)

project(Chapter18Labs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(ch18_ascon_basics)

# Lab 2 is added to the same build when its project is present.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ch18_weather_device/CMakeLists.txt")
    add_subdirectory(ch18_weather_device)
endif()
