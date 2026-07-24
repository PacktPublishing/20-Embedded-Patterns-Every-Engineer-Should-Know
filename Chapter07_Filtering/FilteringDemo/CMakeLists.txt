# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Autumnal Software

add_executable(
    filter_measurements
    main.cpp
)

target_link_libraries(
    filter_measurements
    PRIVATE
        MeasurementValidator
)

target_compile_features(
    filter_measurements
    PRIVATE
        cxx_std_20
)

set(
    FILTERING_DEMO_INSTALL_BINDIR
    "$ENV{HOME}/bin"
    CACHE PATH
    "Installation directory for filtering tools"
)

install(
    TARGETS filter_measurements
    RUNTIME DESTINATION "${FILTERING_DEMO_INSTALL_BINDIR}"
)
