# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

cmake_minimum_required(VERSION 3.22)

project(Chapter04_QueuesBuffersBackpressure LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    include(CTest)
endif()

if(NOT TARGET Common)
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../Common"
        "${CMAKE_CURRENT_BINARY_DIR}/Common"
    )
endif()

# chapter targets below...

project(
    Chapter04_QueuesBuffersBackpressure
    LANGUAGES CXX
)

#add_subdirectory(
    #../Common
    #${CMAKE_CURRENT_BINARY_DIR}/Common
#)

add_subdirectory(QueueMeasurementDemo)
