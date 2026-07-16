# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

cmake_minimum_required(VERSION 3.22)

project(Chapter06_TimingDemo LANGUAGES CXX)

#if(PROJECT_IS_TOP_LEVEL)
#    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
#        set(
#            CMAKE_INSTALL_PREFIX
#            "$ENV{HOME}"
#            CACHE PATH
#            "Default install prefix"
#            FORCE
#        )
#    endif()
#endif()

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    include(CTest)
endif()

add_executable(latency_probe
    latency_probe.cpp
)

target_compile_features(latency_probe
    PRIVATE
        cxx_std_20
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(latency_probe
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

install(
    TARGETS latency_probe
    RUNTIME DESTINATION bin
)

install(
    PROGRAMS
        plot_latency.py
        run_latency_scenario.sh
        run_timing_demo.sh
    DESTINATION bin
)
