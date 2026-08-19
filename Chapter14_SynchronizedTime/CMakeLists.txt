cmake_minimum_required(VERSION 3.22.1)

project(Chapter14SynchronizedTime LANGUAGES CXX)

add_executable(ch14_clock_monitor
    clock_monitor.cpp
)

target_compile_features(ch14_clock_monitor
    PRIVATE
        cxx_std_20
)

target_compile_options(ch14_clock_monitor
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

set(CH14_INSTALL_BINDIR "$ENV{HOME}/bin"
    CACHE PATH "Install directory for Chapter 14 demo executables"
)

install(TARGETS
    ch14_clock_monitor
    RUNTIME DESTINATION "${CH14_INSTALL_BINDIR}"
)
