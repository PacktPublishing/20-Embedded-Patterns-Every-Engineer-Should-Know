add_executable(ch18_ascon_basics
    src/main.cpp
)

target_compile_options(ch18_ascon_basics PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

install(TARGETS ch18_ascon_basics
    RUNTIME DESTINATION bin
)
