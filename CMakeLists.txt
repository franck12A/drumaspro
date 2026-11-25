cmake_minimum_required(VERSION 3.21)

project(
    drumaspro
    VERSION 0.1.0
    DESCRIPTION "Drumaspro engine prototype: virtual instrument, not a DAW"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Keep both configurations available in multi-config generators, ordered so that
# `cmake --build` without --config picks Debug.
if (CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Available build configurations" FORCE)
endif ()

option(DRUMAS_BUILD_TESTS "Build unit tests and register them with CTest" ON)
option(DRUMAS_BUILD_APP "Build the console application" ON)
option(DRUMAS_BUILD_UI "Build the optional Qt/QML user interface" OFF)
option(DRUMAS_WARNINGS_AS_ERRORS "Treat warnings in first-party code as errors" ON)
option(DRUMAS_ENABLE_CLANG_TIDY "Run clang-tidy while compiling first-party code" OFF)
option(DRUMAS_CLANG_TIDY_WARNINGS_AS_ERRORS "Let clang-tidy findings fail the build" OFF)

set(DRUMAS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

if (DRUMAS_BUILD_UI)
    find_package(Qt6 6.8.3 EXACT QUIET COMPONENTS
        Core Gui Qml Quick QuickControls2 QuickLayouts Test)
    if (Qt6_FOUND)
        message(STATUS "Qt ${Qt6_VERSION} found; Qt UI can be built")
    else ()
        message(STATUS "Qt 6.8.3 not found; continuing without the optional UI")
    endif ()
endif ()

# ---------------------------------------------------------------------------
# Compiler settings for first-party targets only. Never applied to third_party.
# ---------------------------------------------------------------------------
add_library(drumas_compile_options INTERFACE)
add_library(drumas::compile_options ALIAS drumas_compile_options)

if (MSVC)
    target_compile_options(
        drumas_compile_options
        INTERFACE $<$<COMPILE_LANGUAGE:CXX>:/W4;/permissive-;/Zc:__cplusplus;/utf-8;/EHsc>
                  $<$<COMPILE_LANGUAGE:C>:/W4;/utf-8>)
    # /MP parallelises compilation of the .cpp files of one project.
    target_compile_options(drumas_compile_options INTERFACE $<$<COMPILE_LANGUAGE:CXX>:/MP>)
    if (DRUMAS_WARNINGS_AS_ERRORS)
        target_compile_options(drumas_compile_options INTERFACE $<$<COMPILE_LANGUAGE:CXX>:/WX>)
    endif ()
else ()
    target_compile_options(drumas_compile_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow)
    if (DRUMAS_WARNINGS_AS_ERRORS)
        target_compile_options(drumas_compile_options INTERFACE -Werror)
    endif ()
endif ()

if (DRUMAS_ENABLE_CLANG_TIDY)
    find_program(DRUMAS_CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY "${DRUMAS_CLANG_TIDY_EXE}" --config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy)
    if (DRUMAS_CLANG_TIDY_WARNINGS_AS_ERRORS)
        list(APPEND CMAKE_CXX_CLANG_TIDY --warnings-as-errors=*)
    endif ()
    message(STATUS "clang-tidy enabled: ${CMAKE_CXX_CLANG_TIDY}")
endif ()

add_subdirectory(third_party)
add_subdirectory(src/core)
add_subdirectory(src/fx)
add_subdirectory(src/music)
add_subdirectory(src/synth)
add_subdirectory(src/engine)
add_subdirectory(src/bridge)

if (DRUMAS_BUILD_APP)
    add_subdirectory(src/app)
endif ()

if (DRUMAS_BUILD_UI AND Qt6_FOUND)
    add_subdirectory(src/ui/qt)
endif ()

if (DRUMAS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()
