cmake_minimum_required(VERSION 3.16)

project(Chapter03ReadinessDemo LANGUAGES CXX)

if(NOT TARGET Common)
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../../Common"
        "${CMAKE_CURRENT_BINARY_DIR}/Common-build"
    )
endif()

add_executable(ch03_nmea_monitor
    nmea_monitor.cpp
)

target_compile_features(ch03_nmea_monitor
    PRIVATE
        cxx_std_20
)

target_compile_options(ch03_nmea_monitor
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_link_libraries(ch03_nmea_monitor
    PRIVATE
        Common
)
