# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

add_library(
    MeasurementValidator
    STATIC
        MeasurementValidator.cpp
        MeasurementValidator.h
)

target_include_directories(
    MeasurementValidator
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(
    MeasurementValidator
    PUBLIC
        cxx_std_20
)
