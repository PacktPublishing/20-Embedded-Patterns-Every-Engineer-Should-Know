cmake_minimum_required(VERSION 3.20)
project(ch20_integrated_weather LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ch20_protocol STATIC ../Chapter15_RemoteMonitoring/monitoring_lab/WeatherMonitoringProtocol.cpp)
target_include_directories(ch20_protocol PUBLIC ../Chapter15_RemoteMonitoring/monitoring_lab)
add_library(ch20_lab INTERFACE)
target_include_directories(ch20_lab INTERFACE include ../Chapter19_AnomalyDetection/include ../Chapter07_Filtering/FilteringDemo)
target_link_libraries(ch20_lab INTERFACE ch20_protocol)

foreach(name weather_device temperature_sensor telemetry_listener command_client)
    add_executable(ch20_${name} src/${name}.cpp)
    target_link_libraries(ch20_${name} PRIVATE ch20_lab)
    target_compile_options(ch20_${name} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

set(CH20_INSTALL_BINDIR "$ENV{HOME}/bin" CACHE PATH "Chapter 20 executable installation directory")
install(TARGETS ch20_weather_device ch20_temperature_sensor ch20_telemetry_listener ch20_command_client
    RUNTIME DESTINATION "${CH20_INSTALL_BINDIR}")
install(PROGRAMS scripts/run_ch20_tmux.sh DESTINATION "${CH20_INSTALL_BINDIR}")
