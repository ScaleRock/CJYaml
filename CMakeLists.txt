cmake_minimum_required(VERSION 3.10)
project(CJYaml LANGUAGES C)

set(SRC src/main/c/src/CJYaml.c)

# Output directories
set(OUT_DIR ${CMAKE_SOURCE_DIR}/out)
set(OUT_DIR_LINUX ${OUT_DIR}/linux)
set(OUT_DIR_WINDOWS ${OUT_DIR}/windows)

# Common flags for GCC/Clang only
set(COMMON_CFLAGS
    -O2 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-overflow
    -Wcast-align -Wpointer-arith -Wundef -Wunreachable-code
    -fstack-protector-strong
)

option(ENABLE_JNI "Enable JNI usage" ON)
option(BUILD_SHARED "Build shared library (.so/.dll)" ON)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(IS_WINDOWS TRUE)
else()
    set(IS_LINUX TRUE)
endif()

find_package(JNI)

if (BUILD_SHARED)
    add_library(cjyaml SHARED ${SRC})
    set_target_properties(cjyaml PROPERTIES OUTPUT_NAME "cjyaml")

    if (MSVC)
        # example: MSVC-specific options (if any)
        target_compile_definitions(cjyaml PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(cjyaml PRIVATE ${COMMON_CFLAGS})
    endif()

    if (JNI_FOUND)
        target_include_directories(cjyaml PRIVATE ${JNI_INCLUDE_DIRS})
        if (DEFINED JNI_LIBRARIES AND JNI_LIBRARIES)
            target_link_libraries(cjyaml PRIVATE ${JNI_LIBRARIES})
        endif()
        target_compile_definitions(cjyaml PRIVATE USE_JNI)
    else()
        message(WARNING "JNI not found; build may fail if JNI is required.")
    endif()

    # Ensure output directories exist
    file(MAKE_DIRECTORY ${OUT_DIR_LINUX})
    file(MAKE_DIRECTORY ${OUT_DIR_WINDOWS})

    if (IS_WINDOWS)
        set_target_properties(cjyaml PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR_WINDOWS}
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUT_DIR_WINDOWS}
        )
    else()
        set_target_properties(cjyaml PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR_LINUX}
            LIBRARY_OUTPUT_DIRECTORY_RELEASE ${OUT_DIR_LINUX}
        )
    endif()
endif()

# Portable clean target
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${OUT_DIR}"
    COMMENT "Remove out/ directory"
)
