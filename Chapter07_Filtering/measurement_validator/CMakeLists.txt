# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

cmake_minimum_required(VERSION 3.22)

project(
    Chapter07Filtering
    LANGUAGES CXX
)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    include(CTest)
endif()

add_subdirectory(MeasurementValidator)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
