cmake_minimum_required(VERSION 3.16)

project(DeviceStatusTests LANGUAGES CXX)

enable_testing()
find_package(GTest REQUIRED)

add_executable(deviceStatusTests
    TestDeviceStatus.cpp
    DeviceStatus.h DeviceStatus.cpp
)

target_compile_features(deviceStatusTests PRIVATE cxx_std_11)

target_link_libraries(deviceStatusTests
    PRIVATE
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(deviceStatusTests)
