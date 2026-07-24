# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

cmake_minimum_required(VERSION 3.22)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(heap_monitor LANGUAGES CXX)
    include(CTest)
endif()

add_library(
    heap_monitor
    STATIC
    src/HeapMonitor.cpp
)

target_include_directories(
    heap_monitor
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include"
)

target_compile_features(heap_monitor PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        heap_monitor
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
    )
elseif(MSVC)
    target_compile_options(heap_monitor PRIVATE /W4 /permissive-)
endif()

if(BUILD_TESTING)
    add_executable(
        heap_monitor_tests
        tests/HeapMonitorTests.cpp
    )

    target_link_libraries(heap_monitor_tests PRIVATE heap_monitor)

    add_test(NAME heap_monitor_tests COMMAND heap_monitor_tests)
endif()
