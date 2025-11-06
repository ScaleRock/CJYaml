cmake_minimum_required(VERSION 3.10)
project(CJYaml C)

set(SRC src/main/c/src/CJYaml.c)
set(OUT_DIR ${CMAKE_SOURCE_DIR}/out)

set(CFLAGS -O2 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-overflow
        -Wcast-align -Wpointer-arith -Wundef -Wunreachable-code
        -fstack-protector-strong -fPIC)

# Kompilacja biblioteki .so dla Linux
add_library(cjyaml_so SHARED ${SRC})
set_target_properties(cjyaml_so PROPERTIES
        OUTPUT_NAME "cjyaml"
        LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}
)
target_compile_options(cjyaml_so PRIVATE ${CFLAGS})

# Kompilacja biblioteki .dll przez MinGW
add_library(cjyaml_dll SHARED ${SRC})
set_target_properties(cjyaml_dll PROPERTIES
        OUTPUT_NAME "cjyaml"
        RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}
)
target_compile_options(cjyaml_dll PRIVATE ${CFLAGS})
set_target_properties(cjyaml_dll PROPERTIES
        CROSSCOMPILING_EMULATOR ""
)

# Ustaw kompilator MinGW jeśli istnieje
find_program(MINGW_GCC x86_64-w64-mingw32-gcc)
if(MINGW_GCC)
    set_target_properties(cjyaml_dll PROPERTIES
            C_COMPILER ${MINGW_GCC}
    )
else()
    message(WARNING "MinGW GCC (x86_64-w64-mingw32-gcc) not found, DLL won't build.")
endif()

# Cel clean
add_custom_target(clean-all
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${OUT_DIR}
)
