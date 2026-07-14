cmake_minimum_required(VERSION 3.22)

project(
    TwentyEmbeddedPatterns
    VERSION 0.1.0
    LANGUAGES CXX
)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build tests" ON)

if(BUILD_TESTING)
    include(CTest)
endif()

function(add_book_subdirectory directory_name)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${directory_name}/CMakeLists.txt")
        message(STATUS "Adding ${directory_name}")
        add_subdirectory("${directory_name}")
    else()
        message(STATUS "Skipping ${directory_name}: no CMakeLists.txt")
    endif()
endfunction()

# Shared libraries/utilities must be added before chapter folders.
file(
    GLOB common_directories
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    CONFIGURE_DEPENDS
    "Common*"
)

list(SORT common_directories COMPARE NATURAL)

foreach(directory_name IN LISTS common_directories)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${directory_name}")
        add_book_subdirectory("${directory_name}")
    endif()
endforeach()

add_book_subdirectory("Tools")

# Chapter folders are added in natural order: Chapter01, Chapter02, ...
file(
    GLOB chapter_directories
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    CONFIGURE_DEPENDS
    "Chapter*"
)

list(SORT chapter_directories COMPARE NATURAL)

foreach(directory_name IN LISTS chapter_directories)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${directory_name}")
        add_book_subdirectory("${directory_name}")
    endif()
endforeach()
