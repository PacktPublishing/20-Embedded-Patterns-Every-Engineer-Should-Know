cmake_minimum_required(VERSION 3.22)

project(CommonLibs LANGUAGES CXX)

# ------------------------------------------------------------
# Common (substrate)
# ------------------------------------------------------------

add_library(Common STATIC
    # Public headers (explicit)
    include/ImmutableByteView.h
    include/MutableByteView.h
    include/NMEAParseStatus.h
    include/NMEAExtractionStream.h
    include/NMEAInsertionStream.h
    include/NMEATokenizer.h
    include/Status.h
    include/EpollReactor.h
    include/UartLineReceiver.h
    include/UdpDatagramReceiver.h
    include/SignalFdSource.h
    include/TimerFdSource.h

    src/NMEAExtractionStream.cpp
    src/NMEAInsertionStream.cpp
    src/NMEATokenizer.cpp
    src/EpollReactor.cpp
    src/UartLineReceiver.cpp
    src/UdpDatagramReceiver.cpp
    src/SignalFdSource.cpp
    src/TimerFdSource.cpp
)

target_include_directories(Common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(Common PUBLIC cxx_std_20)

set_target_properties(Common PROPERTIES
    FOLDER "Common"
)

include(CTest)



add_executable(CommonNmea_Tests
    tests/NmeaTestMain.cpp
)

target_link_libraries(CommonNmea_Tests
    PRIVATE
        Common
)

set_target_properties(CommonNmea_Tests PROPERTIES
    FOLDER "CommonTests"
)

if (BUILD_TESTING)
    add_test(NAME CommonNmea_Tests COMMAND CommonNmea_Tests)
endif()
