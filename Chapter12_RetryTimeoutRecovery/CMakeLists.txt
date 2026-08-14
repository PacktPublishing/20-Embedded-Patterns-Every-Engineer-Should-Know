cmake_minimum_required(VERSION 3.22.1)

project(Chapter12RetryTimeoutRecovery LANGUAGES CXX)

if(NOT TARGET CommonWeatherDevice)
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../Common"
        "${CMAKE_CURRENT_BINARY_DIR}/Common-build"
    )
endif()

add_executable(ch12_retry_client
    retry_lab/retry_client.cpp
)

add_executable(ch12_retry_server
    retry_lab/retry_server.cpp
)

foreach(target ch12_retry_client ch12_retry_server)
    target_compile_features(${target}
        PRIVATE
            cxx_std_20
    )

    target_compile_options(${target}
        PRIVATE
            -Wall
            -Wextra
            -pedantic
    )

    target_link_libraries(${target}
        PRIVATE
            CommonWeatherDevice
    )
endforeach()

set(CH12_INSTALL_BINDIR "$ENV{HOME}/bin"
    CACHE PATH "Install directory for Chapter 12 demo executables"
)

install(TARGETS
    ch12_retry_client
    ch12_retry_server
    RUNTIME DESTINATION "${CH12_INSTALL_BINDIR}"
)

install(PROGRAMS
    retry_lab/run_retry_lab.sh
    DESTINATION "${CH12_INSTALL_BINDIR}"
)
