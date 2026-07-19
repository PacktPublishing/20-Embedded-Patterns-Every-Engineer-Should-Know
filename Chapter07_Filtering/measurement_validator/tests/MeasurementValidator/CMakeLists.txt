# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

add_executable(
    MeasurementValidatorTests
    MeasurementValidatorTests.cpp
)

target_link_libraries(
    MeasurementValidatorTests
    PRIVATE
        MeasurementValidator
)

add_test(
    NAME MeasurementValidatorTests
    COMMAND MeasurementValidatorTests
)
