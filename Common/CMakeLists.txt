cmake_minimum_required(VERSION 3.22.1)

project(CommonLibs LANGUAGES CXX)

include(CTest)

# ------------------------------------------------------------
# Common substrate
# ------------------------------------------------------------

add_library(Common STATIC
    # Core facilities
    include/BoundedQueue.h
    include/ImmutableByteView.h
    include/MutableByteView.h
    include/Status.h

    # NMEA
    include/NMEAParseStatus.h
    include/NMEAExtractionStream.h
    include/NMEAInsertionStream.h
    include/NMEATokenizer.h

    src/NMEAExtractionStream.cpp
    src/NMEAInsertionStream.cpp
    src/NMEATokenizer.cpp

    # Reactor and input sources
    include/EpollReactor.h
    include/UartLineReceiver.h
    include/UdpDatagramReceiver.h
    include/SignalFdSource.h
    include/TimerFdSource.h

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

target_compile_features(Common
    PUBLIC
        cxx_std_20
)

set_target_properties(Common PROPERTIES
    FOLDER "Common"
)

# ------------------------------------------------------------
# Binary Data Stream
# ------------------------------------------------------------

add_library(CommonBds STATIC
    include/BdsCommon.h
    include/BinaryReadStream.h
    include/BinaryWriteStream.h
    include/MessageHeader.h

    src/BdsCommon.cpp
    src/BinaryReadStream.cpp
    src/BinaryWriteStream.cpp
)

target_link_libraries(CommonBds
    PUBLIC
        Common
)

target_include_directories(CommonBds
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(CommonBds
    PUBLIC
        cxx_std_20
)

set_target_properties(CommonBds PROPERTIES
    FOLDER "Common"
)

# ------------------------------------------------------------
# Tests
# ------------------------------------------------------------

if (BUILD_TESTING)
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

    add_test(
        NAME CommonNmea_Tests
        COMMAND CommonNmea_Tests
    )

    add_executable(BoundedQueueTest
        tests/BoundedQueueMain.cpp
    )

    target_link_libraries(BoundedQueueTest
        PRIVATE
            Common
    )

    set_target_properties(BoundedQueueTest PROPERTIES
        FOLDER "CommonTests"
    )

    add_test(
        NAME BoundedQueueTest
        COMMAND BoundedQueueTest
    )

    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/BdsTestMain.cpp")
        add_executable(CommonBds_Tests
            tests/BdsTestMain.cpp
        )

        target_link_libraries(CommonBds_Tests
            PRIVATE
                CommonBds
        )

        set_target_properties(CommonBds_Tests PROPERTIES
            FOLDER "CommonTests"
        )

        add_test(
            NAME CommonBds_Tests
            COMMAND CommonBds_Tests
        )
    endif()
endif()
