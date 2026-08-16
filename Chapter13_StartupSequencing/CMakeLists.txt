cmake_minimum_required(VERSION 3.22.1)

project(Chapter13StartupSequencing LANGUAGES CXX)

if(NOT TARGET Common)
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../Common"
        "${CMAKE_CURRENT_BINARY_DIR}/Common-build"
    )
endif()

find_package(Threads REQUIRED)

add_executable(ch13_startup_lab
    startup_lab/main.cpp
)

target_compile_features(ch13_startup_lab
    PRIVATE
        cxx_std_20
)

target_compile_options(ch13_startup_lab
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_link_libraries(ch13_startup_lab
    PRIVATE
        Common
        Threads::Threads
)

set(CH13_INSTALL_BINDIR "$ENV{HOME}/bin"
    CACHE PATH "Install directory for Chapter 13 demo executable"
)

install(TARGETS
    ch13_startup_lab
    RUNTIME DESTINATION "${CH13_INSTALL_BINDIR}"
)

install(PROGRAMS
    startup_lab/run_startup_lab.sh
    DESTINATION "${CH13_INSTALL_BINDIR}"
)
