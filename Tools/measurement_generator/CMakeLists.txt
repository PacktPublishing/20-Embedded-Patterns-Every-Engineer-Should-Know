# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

cmake_minimum_required(VERSION 3.22)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(measurement_generator LANGUAGES CXX)
endif()

add_executable(
    measurement_generator
    main.cpp
    MeasurementGenerator.cpp
)

target_compile_features(measurement_generator PRIVATE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        measurement_generator
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
    )
elseif(MSVC)
    target_compile_options(measurement_generator PRIVATE /W4 /permissive-)
endif()

set(
    MEASUREMENT_GENERATOR_INSTALL_BINDIR
    "$ENV{HOME}/bin"
    CACHE PATH "Install directory for measurement generator tools"
)

install(
    TARGETS measurement_generator
    RUNTIME DESTINATION "${MEASUREMENT_GENERATOR_INSTALL_BINDIR}"
)

install(
    PROGRAMS "${CMAKE_CURRENT_LIST_DIR}/plot_measurements.py"
    DESTINATION "${MEASUREMENT_GENERATOR_INSTALL_BINDIR}"
)
