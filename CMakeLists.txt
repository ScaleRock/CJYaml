cmake_minimum_required(VERSION 3.10)
project(CJYaml LANGUAGES C)

# Source files
set(SRC src/main/c/src/CJYaml.c)

# Output directories by platform
set(OUT_DIR ${CMAKE_SOURCE_DIR}/out)
set(OUT_DIR_LINUX ${OUT_DIR}/linux)
set(OUT_DIR_WINDOWS ${OUT_DIR}/windows)

# Common compiler flags (list)
set(COMMON_CFLAGS
        -O2
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wstrict-overflow
        -Wcast-align
        -Wpointer-arith
        -Wundef
        -Wunreachable-code
        -fstack-protector-strong
)

# Options
option(ENABLE_JNI "Enable JNI usage (set OFF to build without JNI)" ON)
option(BUILD_SHARED "Also build shared library (platform default: .so/.dll)" ON)

# Allow user to point to a Windows JDK root for cross-builds
set(JAVA_WINDOWS_HOME "$ENV{JAVA_WINDOWS_HOME}" CACHE PATH "Path to Windows JDK root (for cross-builds)")
if (NOT JAVA_WINDOWS_HOME AND DEFINED ENV{JAVA_HOME_WINDOWS})
    set(JAVA_WINDOWS_HOME "$ENV{JAVA_HOME_WINDOWS}")
endif()

# Detect platform / cross-compilation
message(STATUS "CMake system name: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMake crosscompiling: ${CMAKE_CROSSCOMPILING}")

set(IS_NATIVE_LINUX FALSE)
set(IS_MINGW_CROSS  FALSE)
set(IS_NATIVE_WINDOWS FALSE)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (CMAKE_CROSSCOMPILING)
        set(IS_MINGW_CROSS TRUE)
    else()
        set(IS_NATIVE_WINDOWS TRUE)
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (NOT CMAKE_CROSSCOMPILING)
        set(IS_NATIVE_LINUX TRUE)
    endif()
endif()

# --- JNI detection with cross-build (MinGW) support ---------------------------
set(JNI_FOUND FALSE)
if (ENABLE_JNI)
    if (IS_NATIVE_LINUX OR IS_NATIVE_WINDOWS)
        # Native: let CMake find JNI
        find_package(JNI)
        if (JNI_FOUND)
            message(STATUS "Native JNI found via find_package. JNI include dirs: ${JNI_INCLUDE_DIRS}")
        else()
            # some CMake versions don't set JNI_FOUND; check include var as fallback
            if (DEFINED JNI_INCLUDE_DIRS AND JNI_INCLUDE_DIRS)
                set(JNI_FOUND TRUE)
                message(STATUS "Native JNI (fallback) - JNI include dirs: ${JNI_INCLUDE_DIRS}")
            endif()
        endif()
    elseif (IS_MINGW_CROSS)
        # Cross-building to Windows: try user-supplied JAVA_WINDOWS_HOME or a few heuristic locations
        if (JAVA_WINDOWS_HOME)
            message(STATUS "Cross-build (Windows): using JAVA_WINDOWS_HOME=${JAVA_WINDOWS_HOME} for JNI")
            set(JNI_INCLUDE_DIRS "${JAVA_WINDOWS_HOME}/include;${JAVA_WINDOWS_HOME}/include/win32" CACHE STRING "JNI include dirs for Windows")
            if (EXISTS "${JAVA_WINDOWS_HOME}/include/jni.h")
                set(JNI_FOUND TRUE)
            else()
                message(WARNING "JAVA_WINDOWS_HOME provided but include/jni.h not found under ${JAVA_WINDOWS_HOME}")
            endif()

            # try set JNI_LIBRARIES if present
            if (EXISTS "${JAVA_WINDOWS_HOME}/lib/jvm.lib")
                set(JNI_LIBRARIES "${JAVA_WINDOWS_HOME}/lib/jvm.lib" CACHE FILEPATH "JNI import lib (jvm.lib)")
            elseif (EXISTS "${JAVA_WINDOWS_HOME}/lib/server/jvm.lib")
                set(JNI_LIBRARIES "${JAVA_WINDOWS_HOME}/lib/server/jvm.lib" CACHE FILEPATH "JNI import lib (jvm.lib)")
            endif()
        else()
            set(_try_paths
                    "${CMAKE_SOURCE_DIR}/third_party/win-jdk"
                    "${CMAKE_SOURCE_DIR}/third_party/windows-jdk"
                    "/opt/windows-jdk"
                    "/usr/local/windows-jdk"
                    "/usr/lib/jvm/windows"
            )
            foreach(p IN LISTS _try_paths)
                if (EXISTS "${p}/include/jni.h")
                    message(STATUS "Found Windows JDK include at: ${p}")
                    set(JNI_INCLUDE_DIRS "${p}/include;${p}/include/win32" CACHE STRING "JNI include dirs (auto-found)")
                    if (EXISTS "${p}/lib/jvm.lib")
                        set(JNI_LIBRARIES "${p}/lib/jvm.lib" CACHE FILEPATH "JNI libraries (auto-found)")
                    elseif (EXISTS "${p}/lib/server/jvm.lib")
                        set(JNI_LIBRARIES "${p}/lib/server/jvm.lib" CACHE FILEPATH "JNI libraries (auto-found)")
                    endif()
                    set(JNI_FOUND TRUE)
                    break()
                endif()
            endforeach()
            if (NOT JNI_FOUND)
                message(STATUS "No Windows JDK auto-found. If you want JNI with cross-build, set -DJAVA_WINDOWS_HOME=/path/to/win-jdk or add one to third_party/win-jdk")
            endif()
        endif()
    else()
        # unknown platform: try native find_package as fallback
        find_package(JNI)
        if (JNI_FOUND OR (DEFINED JNI_INCLUDE_DIRS AND JNI_INCLUDE_DIRS))
            set(JNI_FOUND TRUE)
        endif()
    endif()

    if (JNI_FOUND)
        message(STATUS "JNI enabled. JNI include dirs: ${JNI_INCLUDE_DIRS}")
        if (DEFINED JNI_LIBRARIES AND JNI_LIBRARIES)
            message(STATUS "JNI libraries: ${JNI_LIBRARIES}")
        else()
            message(WARNING "JNI headers found but JNI_LIBRARIES not set; linking may fail if you need jvm import lib.")
        endif()
    else()
        message(STATUS "JNI not found or disabled; building without JNI.")
    endif()
else()
    message(STATUS "JNI explicitly disabled by ENABLE_JNI=OFF.")
endif()
# ------------------------------------------------------------------------------

# ------------------------
# STATIC library (primary)
# ------------------------
add_library(cjyaml_static STATIC ${SRC})
set_target_properties(cjyaml_static PROPERTIES
        OUTPUT_NAME "cjyaml"
        ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR}          # will be overridden below per-platform
)

target_compile_options(cjyaml_static PRIVATE ${COMMON_CFLAGS})

if (JNI_FOUND)
    target_include_directories(cjyaml_static PRIVATE ${JNI_INCLUDE_DIRS})
    if (DEFINED JNI_LIBRARIES AND JNI_LIBRARIES)
        target_link_libraries(cjyaml_static PRIVATE ${JNI_LIBRARIES})
    endif()
    target_compile_definitions(cjyaml_static PRIVATE USE_JNI)
endif()

# Place platform-specific outputs into subfolders:
if (IS_NATIVE_WINDOWS OR IS_MINGW_CROSS)
    set_target_properties(cjyaml_static PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR_WINDOWS})
else()
    set_target_properties(cjyaml_static PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR_LINUX})
endif()

# For multi-config generators, also set config-specific dirs (DEBUG/RELEASE)
foreach(cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    if (IS_NATIVE_WINDOWS OR IS_MINGW_CROSS)
        set_property(TARGET cjyaml_static PROPERTY ARCHIVE_OUTPUT_DIRECTORY_${cfg} ${OUT_DIR_WINDOWS})
    else()
        set_property(TARGET cjyaml_static PROPERTY ARCHIVE_OUTPUT_DIRECTORY_${cfg} ${OUT_DIR_LINUX})
    endif()
endforeach()

# ------------------------
# Optional SHARED library
# ------------------------
if (BUILD_SHARED)
    add_library(cjyaml_shared SHARED ${SRC})
    set_target_properties(cjyaml_shared PROPERTIES OUTPUT_NAME "cjyaml")
    target_compile_options(cjyaml_shared PRIVATE ${COMMON_CFLAGS})

    if (JNI_FOUND)
        target_include_directories(cjyaml_shared PRIVATE ${JNI_INCLUDE_DIRS})
        if (DEFINED JNI_LIBRARIES AND JNI_LIBRARIES)
            target_link_libraries(cjyaml_shared PRIVATE ${JNI_LIBRARIES})
        endif()
        target_compile_definitions(cjyaml_shared PRIVATE USE_JNI)
    endif()

    if (IS_NATIVE_WINDOWS OR IS_MINGW_CROSS)
        set_target_properties(cjyaml_shared PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR_WINDOWS})
        foreach(cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
            set_property(TARGET cjyaml_shared PROPERTY RUNTIME_OUTPUT_DIRECTORY_${cfg} ${OUT_DIR_WINDOWS})
        endforeach()
    else()
        set_target_properties(cjyaml_shared PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR_LINUX})
        foreach(cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
            set_property(TARGET cjyaml_shared PROPERTY LIBRARY_OUTPUT_DIRECTORY_${cfg} ${OUT_DIR_LINUX})
        endforeach()
    endif()
endif()

# ------------------------
# Convenience targets
# ------------------------
add_custom_target(clean-all
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${OUT_DIR}
        COMMENT "Remove out/ directory"
)

add_custom_target(package-linux
        COMMENT "Use a native linux build dir: cmake -S . -B build && cmake --build build --config Release"
)
add_custom_target(package-windows
        COMMENT "Cross-build for Windows with MinGW or run on Windows"
)

message(STATUS "Static archive will be placed in: ${OUT_DIR_LINUX} (native) or ${OUT_DIR_WINDOWS} (cross)")
if (BUILD_SHARED)
    message(STATUS "Shared library output (if built) configured per-platform")
endif()
