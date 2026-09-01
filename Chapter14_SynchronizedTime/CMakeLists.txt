cmake_minimum_required(VERSION 3.22.1)

project(Chapter14SynchronizedTime LANGUAGES CXX)

if(NOT TARGET Common)
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../Common"
        "${CMAKE_CURRENT_BINARY_DIR}/Common-build"
    )
endif()

find_package(Threads REQUIRED)

function(ch14_configure_target target)
    target_compile_features(${target}
        PRIVATE
            cxx_std_20
    )

    target_compile_options(${target}
        PRIVATE
            -Wall
            -Wextra
            -pedantic
    )
endfunction()

# Lab 1 clock monitor.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/clock_monitor.cpp")
    add_executable(ch14_clock_monitor
        clock_monitor.cpp
    )
    ch14_configure_target(ch14_clock_monitor)
endif()

# Lab 2: Chronos remote measurement simulator.
add_executable(ch14_temperature_tx
    temperature_tx.cpp
    SynchronizedMeasurementProtocol.h
)
ch14_configure_target(ch14_temperature_tx)
target_link_libraries(ch14_temperature_tx
    PRIVATE
        CommonBds
)

# Lab 2: Dev UDP receiver + local acquisition + timestamp fusion.
add_executable(ch14_synchronized_fusion
    synchronized_fusion.cpp
    SynchronizedMeasurementProtocol.h
)
ch14_configure_target(ch14_synchronized_fusion)
target_link_libraries(ch14_synchronized_fusion
    PRIVATE
        CommonBds
        Threads::Threads
)

set(CH14_INSTALL_BINDIR "$ENV{HOME}/bin"
    CACHE PATH "Install directory for Chapter 14 demo executables and scripts"
)

set(CH14_INSTALL_TARGETS
    ch14_temperature_tx
    ch14_synchronized_fusion
)

if(TARGET ch14_clock_monitor)
    list(APPEND CH14_INSTALL_TARGETS ch14_clock_monitor)
endif()

install(TARGETS ${CH14_INSTALL_TARGETS}
    RUNTIME DESTINATION "${CH14_INSTALL_BINDIR}"
)

install(PROGRAMS
    "${CMAKE_CURRENT_LIST_DIR}/run_measurement_lab.sh"
    DESTINATION "${CH14_INSTALL_BINDIR}"
)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/run_ptp_lab.sh")
    install(PROGRAMS
        "${CMAKE_CURRENT_LIST_DIR}/run_ptp_lab.sh"
        DESTINATION "${CH14_INSTALL_BINDIR}"
    )
endif()
