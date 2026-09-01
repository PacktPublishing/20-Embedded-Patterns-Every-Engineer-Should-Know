cmake_minimum_required(VERSION 3.22.1)

project(Chapter15RemoteMonitoringControl LANGUAGES CXX)

add_library(ch15_monitoring_protocol
    monitoring_lab/WeatherMonitoringProtocol.cpp
)

target_include_directories(ch15_monitoring_protocol
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/monitoring_lab
)

target_compile_features(ch15_monitoring_protocol
    PUBLIC
        cxx_std_20
)

target_compile_options(ch15_monitoring_protocol
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

add_executable(ch15_weather_device
    monitoring_lab/weather_device.cpp
)

target_compile_features(ch15_weather_device PRIVATE cxx_std_20)
target_compile_options(ch15_weather_device PRIVATE -Wall -Wextra -pedantic)
target_link_libraries(ch15_weather_device PRIVATE ch15_monitoring_protocol)

add_executable(ch15_remote_monitor
    monitoring_lab/remote_monitor.cpp
)

target_compile_features(ch15_remote_monitor PRIVATE cxx_std_20)
target_compile_options(ch15_remote_monitor PRIVATE -Wall -Wextra -pedantic)
target_link_libraries(ch15_remote_monitor PRIVATE ch15_monitoring_protocol)

include(CTest)
if(BUILD_TESTING)
    add_executable(ch15_protocol_tests
        tests/protocol_tests.cpp
    )
    target_compile_features(ch15_protocol_tests PRIVATE cxx_std_20)
    target_compile_options(ch15_protocol_tests PRIVATE -Wall -Wextra -pedantic)
    target_link_libraries(ch15_protocol_tests PRIVATE ch15_monitoring_protocol)
    add_test(NAME ch15_protocol_tests COMMAND ch15_protocol_tests)
endif()

set(CH15_INSTALL_BINDIR "$ENV{HOME}/bin"
    CACHE PATH "Install directory for Chapter 15 lab executables and scripts"
)

install(TARGETS
    ch15_weather_device
    ch15_remote_monitor
    RUNTIME DESTINATION "${CH15_INSTALL_BINDIR}"
)

install(PROGRAMS
    monitoring_lab/run_monitoring_lab.sh
    DESTINATION "${CH15_INSTALL_BINDIR}"
)
