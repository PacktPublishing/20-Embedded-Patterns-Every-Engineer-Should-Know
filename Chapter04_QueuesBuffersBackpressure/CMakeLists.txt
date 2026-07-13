# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

cmake_minimum_required(VERSION 3.22)

project(
    Chapter04_QueuesBuffersBackpressure
    LANGUAGES CXX
)

add_subdirectory(
    ../Common
    ${CMAKE_CURRENT_BINARY_DIR}/Common
)

add_subdirectory(QueueMeasurementDemo)
