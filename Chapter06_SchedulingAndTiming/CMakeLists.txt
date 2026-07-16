# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

cmake_minimum_required(VERSION 3.22)

project(Chapter06_SchedulingAndTiming LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(
            CMAKE_INSTALL_PREFIX
            "$ENV{HOME}"
            CACHE PATH
            "Default install prefix"
            FORCE
        )
    endif()

    include(CTest)
endif()

add_subdirectory(timing_demo)
