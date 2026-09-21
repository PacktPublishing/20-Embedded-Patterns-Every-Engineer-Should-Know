set(CH18_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Common"
    CACHE PATH "Path to the book Common directory")
set(CH18_PERSISTENCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Chapter17_DataPersistence"
    CACHE PATH "Path to the Chapter 17 persistence lab")

if(NOT TARGET CommonBds)
    add_subdirectory("${CH18_COMMON_DIR}" "${CMAKE_BINARY_DIR}/Common")
endif()

add_executable(ch18_weather_device
    src/main.cpp
)

target_include_directories(ch18_weather_device PRIVATE
    "${CH18_PERSISTENCE_DIR}/include"
)

target_link_libraries(ch18_weather_device PRIVATE
    CommonBds
)

target_compile_options(ch18_weather_device PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

install(TARGETS ch18_weather_device
    RUNTIME DESTINATION bin
)
