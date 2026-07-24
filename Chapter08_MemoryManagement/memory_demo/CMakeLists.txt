# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

cmake_minimum_required(VERSION 3.22)

project(memory_demo LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

if(NOT TARGET heap_monitor)
    find_path(
        HEAP_MONITOR_INCLUDE_DIR
        NAMES heap_monitor/HeapMonitor.h
        REQUIRED
    )
    find_library(
        HEAP_MONITOR_LIBRARY
        NAMES heap_monitor
        REQUIRED
    )

    add_library(heap_monitor STATIC IMPORTED)
    set_target_properties(
        heap_monitor
        PROPERTIES
            IMPORTED_LOCATION "${HEAP_MONITOR_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${HEAP_MONITOR_INCLUDE_DIR}"
    )
endif()

add_executable(memory_demo main.cpp)
target_link_libraries(memory_demo PRIVATE heap_monitor)
target_compile_features(memory_demo PRIVATE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        memory_demo
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
    )
elseif(MSVC)
    target_compile_options(memory_demo PRIVATE /W4 /permissive-)
endif()

install(TARGETS memory_demo RUNTIME DESTINATION bin)
install(PROGRAMS run_memory_demo.sh DESTINATION bin)
