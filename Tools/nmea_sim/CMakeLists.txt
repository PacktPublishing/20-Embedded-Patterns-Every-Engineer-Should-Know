cmake_minimum_required(VERSION 3.16)

project(nmea_sim
    VERSION 1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(nmea_sim
    main.cpp
    NmeaSimApp.cpp
    NmeaSentence.cpp
    StreamScheduler.cpp
    UartTransmitEndpoint.cpp
    UdpTransmitEndpoint.cpp
    FileTransmitEndpoint.cpp
    StdoutTransmitEndpoint.cpp
    ValueGenerator.cpp
)

target_include_directories(nmea_sim
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(nmea_sim
    PRIVATE
        cxx_std_20
)

if (MSVC)
    target_compile_options(nmea_sim PRIVATE /W4)
else()
    target_compile_options(nmea_sim PRIVATE -Wall -Wextra -Wpedantic)
endif()

set(NMEA_SIM_INSTALL_BINDIR "$ENV{HOME}/bin"
    CACHE PATH "Install directory for nmea_sim"
)

install(TARGETS nmea_sim
    RUNTIME DESTINATION "${NMEA_SIM_INSTALL_BINDIR}"
)
