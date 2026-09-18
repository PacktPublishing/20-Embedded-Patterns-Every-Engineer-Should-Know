cmake_minimum_required(VERSION 3.20)
project(ch17_data_persistence LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

set(CH17_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Common"
    CACHE PATH "Path to the book Common directory")
set(CH17_PARAMETER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Chapter16_Parameter_Management"
    CACHE PATH "Path to the Chapter 16 parameter lab")

if(NOT TARGET CommonBds)
    add_subdirectory("${CH17_COMMON_DIR}" "${CMAKE_BINARY_DIR}/Common")
endif()

add_library(ch17_persistence STATIC src/ParameterStorageImage.cpp)
target_include_directories(ch17_persistence
    PUBLIC include "${CH17_PARAMETER_DIR}/include")
target_link_libraries(ch17_persistence PUBLIC CommonBds Common)
target_compile_options(ch17_persistence PRIVATE -Wall -Wextra -Wpedantic)

add_executable(ch17_weather_device src/WeatherDeviceMain.cpp)
target_link_libraries(ch17_weather_device PRIVATE ch17_persistence)
target_compile_options(ch17_weather_device PRIVATE -Wall -Wextra -Wpedantic)

add_executable(ch17_image_inspector src/ImageInspectorMain.cpp)
target_link_libraries(ch17_image_inspector PRIVATE ch17_persistence)
target_compile_options(ch17_image_inspector PRIVATE -Wall -Wextra -Wpedantic)

if(BUILD_TESTING)
    add_executable(ch17_persistence_tests tests/PersistenceTests.cpp)
    target_link_libraries(ch17_persistence_tests PRIVATE ch17_persistence)
    target_compile_options(ch17_persistence_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME ch17_persistence_tests COMMAND ch17_persistence_tests)
endif()

set(CH17_INSTALL_BINDIR "$ENV{HOME}/bin"
    CACHE PATH "Install directory for Chapter 17 lab executables and script")

install(TARGETS
    ch17_weather_device
    ch17_image_inspector
    RUNTIME DESTINATION "${CH17_INSTALL_BINDIR}")

install(PROGRAMS
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_persistence_lab.sh"
    DESTINATION "${CH17_INSTALL_BINDIR}")
