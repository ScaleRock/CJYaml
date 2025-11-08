cmake_minimum_required(VERSION 3.10)
project(CJYaml LANGUAGES C)

# Source files
set(SRC src/main/c/src/CJYaml.c)

# Output directories by platform
set(OUT_DIR ${CMAKE_SOURCE_DIR}/out)
set(OUT_DIR_LINUX ${OUT_DIR}/linux)
set(OUT_DIR_WINDOWS ${OUT_DIR}/windows)

# Common compiler flags
set(COMMON_CFLAGS
    -O2 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-overflow
    -Wcast-align -Wpointer-arith -Wundef -Wunreachable-code
    -fstack-protector-strong
)

# Options
option(ENABLE_JNI "Enable JNI usage" ON)
option(BUILD_SHARED "Build shared library (.so/.dll)" ON)

# Detect platform
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(IS_WINDOWS TRUE)
else()
    set(IS_LINUX TRUE)
endif()

# JNI detection
if (ENABLE_JNI)
    find_package(JNI)
    if (JNI_FOUND)
        message(STATUS "JNI include dirs: ${JNI_INCLUDE_DIRS}")
    else()
        message(WARNING "JNI not found; build may fail if JNI is used")
    endif()
endif()

# ------------------------
# ONLY shared library
# ------------------------
if (BUILD_SHARED)
    add_library(cjyaml SHARED ${SRC})
    set_target_properties(cjyaml PROPERTIES OUTPUT_NAME "cjyaml")
    target_compile_options(cjyaml PRIVATE ${COMMON_CFLAGS})

    if (JNI_FOUND)
        target_include_directories(cjyaml PRIVATE ${JNI_INCLUDE_DIRS})
        if (DEFINED JNI_LIBRARIES AND JNI_LIBRARIES)
            target_link_libraries(cjyaml PRIVATE ${JNI_LIBRARIES})
        endif()
        target_compile_definitions(cjyaml PRIVATE USE_JNI)
    endif()

    # Output directories
    if (IS_WINDOWS)
        set_target_properties(cjyaml PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR_WINDOWS})
    else()
        set_target_properties(cjyaml PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR_LINUX})
    endif()
endif()

# ------------------------
# Convenience targets
# ------------------------
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${OUT_DIR}
    COMMENT "Remove out/ directory"
)
