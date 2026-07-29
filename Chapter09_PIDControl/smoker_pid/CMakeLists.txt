cmake_minimum_required(VERSION 3.20)
project(smoker_pid LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(smoker_control
    src/PidController.cpp
    src/SmokerPlant.cpp
    src/Simulation.cpp
)
target_include_directories(smoker_control PUBLIC include)
target_compile_options(smoker_control PRIVATE -Wall -Wextra -Wpedantic)

add_executable(smoker_sim src/main.cpp)
target_link_libraries(smoker_sim PRIVATE smoker_control)
target_compile_options(smoker_sim PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    add_executable(test_pid_controller
        tests/test_pid_controller.cpp
        src/PidController.cpp
    )
    target_include_directories(test_pid_controller PRIVATE include)
    target_compile_options(test_pid_controller PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME pid_controller COMMAND test_pid_controller)

    add_executable(test_smoker_plant
        tests/test_smoker_plant.cpp
        src/SmokerPlant.cpp
    )
    target_include_directories(test_smoker_plant PRIVATE include)
    target_compile_options(test_smoker_plant PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME smoker_plant COMMAND test_smoker_plant)
endif()

set(CH09_INSTALL_BINDIR "$ENV{HOME}/bin"
    CACHE PATH "Install directory for Chapter 9 demo executables and scripts"
)

set(CH09_RUN_DEMO_SCRIPT
    "${CMAKE_CURRENT_LIST_DIR}/scripts/run_pid_demo.sh"
)

if(NOT EXISTS "${CH09_RUN_DEMO_SCRIPT}")
    message(FATAL_ERROR "Missing script: ${CH09_RUN_DEMO_SCRIPT}")
endif()

install(TARGETS smoker_sim
    RUNTIME DESTINATION "${CH09_INSTALL_BINDIR}"
)

install(PROGRAMS
    "${CH09_RUN_DEMO_SCRIPT}"
    DESTINATION "${CH09_INSTALL_BINDIR}"
)
