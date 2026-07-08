cmake_minimum_required(VERSION 3.16)

project(Chapter03EventLoops LANGUAGES CXX)

if(NOT TARGET Common)
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../Common"
        "${CMAKE_CURRENT_BINARY_DIR}/Common-build"
    )
endif()

add_executable(ch03_nmea_monitor
    readiness_demo/nmea_monitor.cpp
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

set(CH03_INSTALL_BINDIR "$ENV{HOME}/bin"
    CACHE PATH "Install directory for Chapter 3 demo executables and scripts"
)

set(CH03_RUN_SOURCES_SCRIPT
    "${CMAKE_CURRENT_LIST_DIR}/run_nmea_sources.sh"
)

set(CH03_READINESS_DEMO_SCRIPT
    "${CMAKE_CURRENT_LIST_DIR}/ch03_readiness_demo.sh"
)

if(NOT EXISTS "${CH03_RUN_SOURCES_SCRIPT}")
    message(FATAL_ERROR "Missing script: ${CH03_RUN_SOURCES_SCRIPT}")
endif()

if(NOT EXISTS "${CH03_READINESS_DEMO_SCRIPT}")
    message(FATAL_ERROR "Missing script: ${CH03_READINESS_DEMO_SCRIPT}")
endif()

install(TARGETS ch03_nmea_monitor
    RUNTIME DESTINATION "${CH03_INSTALL_BINDIR}"
)

install(PROGRAMS
    "${CH03_RUN_SOURCES_SCRIPT}"
    "${CH03_READINESS_DEMO_SCRIPT}"
    DESTINATION "${CH03_INSTALL_BINDIR}"
)
