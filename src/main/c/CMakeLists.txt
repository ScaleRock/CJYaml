cmake_minimum_required(VERSION 3.10)
project(CJYaml C)

# Zakładamy, że CMakeLists.txt jest w src/main/c/
set(SRC ${CMAKE_SOURCE_DIR}/src/CJYaml.c)   # => src/main/c/src/CJYaml.c on disk
set(OUT_DIR ${CMAKE_SOURCE_DIR}/out)        # => src/main/c/out

set(CFLAGS -O2 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-overflow
        -Wcast-align -Wpointer-arith -Wundef -Wunreachable-code
        -fstack-protector-strong -fPIC)

# Jeden target - biblioteka współdzielona (so/dll)
add_library(cjyaml SHARED ${SRC})

set_target_properties(cjyaml PROPERTIES
        OUTPUT_NAME "cjyaml"
        LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}   # .so
        RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}   # .dll dla cross
)

target_compile_options(cjyaml PRIVATE ${CFLAGS})

# Utwórz out podczas konfiguracji jeśli nie istnieje
file(MAKE_DIRECTORY ${OUT_DIR})

# Opcjonalny target do czyszczenia
add_custom_target(clean-out
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${OUT_DIR}
        COMMENT "Usuwanie katalogu out"
)
