# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

find_package(Threads REQUIRED)

add_executable(
    QueueMeasurementDemo
    main.cpp
)

target_compile_features(
    QueueMeasurementDemo
    PRIVATE
        cxx_std_20
)

target_include_directories(
    QueueMeasurementDemo
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Common/include
)

target_compile_options(
    QueueMeasurementDemo
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_link_libraries(
    QueueMeasurementDemo
    PRIVATE
        Common
        Threads::Threads
)
