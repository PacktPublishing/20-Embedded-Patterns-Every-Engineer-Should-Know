cmake_minimum_required(VERSION 3.20)
project(ch16_parameter_management LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(Threads REQUIRED)

set(CH16_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Common"
    CACHE PATH "Path to the book Common directory")

if(NOT TARGET CommonBds)
    if(NOT EXISTS "${CH16_COMMON_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "Could not find Common at ${CH16_COMMON_DIR}. "
            "Place this lab beside the book Common directory or set CH16_COMMON_DIR.")
    endif()

    add_subdirectory(
        "${CH16_COMMON_DIR}"
        "${CMAKE_BINARY_DIR}/Common")
endif()

add_library(ch16_parameter_core INTERFACE)
target_include_directories(ch16_parameter_core
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ch16_parameter_core
    INTERFACE
        CommonBds
        Common)
target_compile_features(ch16_parameter_core INTERFACE cxx_std_20)

add_executable(ch16_parameter_lab
    src/main.cpp)
target_link_libraries(ch16_parameter_lab
    PRIVATE
        ch16_parameter_core
        Threads::Threads)
target_compile_options(ch16_parameter_lab
    PRIVATE
        -Wall -Wextra -Wpedantic)

if(BUILD_TESTING)
    add_executable(ch16_parameter_tests
        tests/test_parameter_system.cpp)
    target_link_libraries(ch16_parameter_tests
        PRIVATE
            ch16_parameter_core
            Threads::Threads)
    target_compile_options(ch16_parameter_tests
        PRIVATE
            -Wall -Wextra -Wpedantic)

    add_test(
        NAME ch16_parameter_tests
        COMMAND ch16_parameter_tests)
endif()

set(CH16_INSTALL_BINDIR "$ENV{HOME}/bin"
    CACHE PATH "Install directory for Chapter 16 lab executable and script")

install(TARGETS ch16_parameter_lab
    RUNTIME DESTINATION "${CH16_INSTALL_BINDIR}")

install(PROGRAMS
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_parameter_lab.sh"
    DESTINATION "${CH16_INSTALL_BINDIR}")
