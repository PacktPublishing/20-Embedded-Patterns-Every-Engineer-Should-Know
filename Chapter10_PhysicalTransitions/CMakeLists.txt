cmake_minimum_required(VERSION 3.20)

project(process_oven LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ramping
    src/Ramper.cpp
    src/RampDriver.cpp
)

target_include_directories(ramping PUBLIC include)

target_compile_options(ramping PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(process_oven
    src/main.cpp
)

target_link_libraries(process_oven PRIVATE ramping)

target_compile_options(process_oven PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

enable_testing()

add_executable(ramping_tests
    tests/ramping_tests.cpp
)

target_link_libraries(ramping_tests PRIVATE ramping)

add_test(
    NAME ramping_tests
    COMMAND ramping_tests
)

set(CH10_INSTALL_DIR "$ENV{HOME}/bin"
    CACHE PATH "Installation directory for Chapter 10 executables"
)

install(
    TARGETS process_oven
    RUNTIME DESTINATION "${CH10_INSTALL_DIR}"
)

install(
    PROGRAMS plot_process_oven.py
    DESTINATION "${CH10_INSTALL_DIR}"
)
