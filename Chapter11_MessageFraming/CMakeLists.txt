cmake_minimum_required(VERSION 3.22.1)

project(Chapter11SimpleCodec LANGUAGES CXX)

# When this directory is built independently, bring in the repository's
# shared Common library. A parent project may already have added it.
if (NOT TARGET Common)
    set(PBOOK_COMMON_DIR
        "${CMAKE_CURRENT_LIST_DIR}/../Common"
        CACHE PATH
        "Path to the Packt book Common library")

    add_subdirectory(
        "${PBOOK_COMMON_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/Common")
endif()

add_executable(simple_codec
    src/SimpleCodecMain.cpp
    include/WeatherObservation.h
    include/WeatherObservationCodec.h
)

target_include_directories(simple_codec
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(simple_codec
    PRIVATE
        CommonBds
)

target_compile_features(simple_codec PRIVATE cxx_std_20)

include(CTest)

if (BUILD_TESTING)
    add_test(NAME Chapter11_SimpleCodec COMMAND simple_codec)
endif()
