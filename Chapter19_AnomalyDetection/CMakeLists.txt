cmake_minimum_required(VERSION 3.22.1)

project(Chapter19_LightweightAnomalyDetection LANGUAGES CXX)

include(CTest)

add_executable(ch19_weather_device
    src/main.cpp
)

target_include_directories(ch19_weather_device
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ch19_weather_device PRIVATE cxx_std_20)

install(
    TARGETS ch19_weather_device
    RUNTIME DESTINATION "$ENV{HOME}/bin"
)

if(BUILD_TESTING)
    add_executable(ch19_anomaly_tests
        tests/AnomalyDetectionTests.cpp
    )

    target_include_directories(ch19_anomaly_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_features(ch19_anomaly_tests PRIVATE cxx_std_20)

    add_test(NAME ch19_anomaly_tests COMMAND ch19_anomaly_tests)
endif()
