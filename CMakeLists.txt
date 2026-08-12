# atc_menu — builds the distributable static library: lib/libatc_menu.a
#
#   cmake --preset default && cmake --build --preset default
#   (cd build/default && cpack)

cmake_minimum_required(VERSION 3.22)

# MAJOR.MINOR.PATCH come from the latest vMAJOR.MINOR.PATCH tag; the describe
# string adds commits-since-tag and dirty state, so an artifact traces back to
# a commit. No tag, no git: the fallback below.
set(ATC_MENU_VERSION_FALLBACK "0.1.0")
find_package(Git QUIET)
set(ATC_MENU_GIT_DESCRIBE "")
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty --match "v[0-9]*.[0-9]*.[0-9]*"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE ATC_MENU_GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()

if(ATC_MENU_GIT_DESCRIBE MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    set(ATC_MENU_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
else()
    set(ATC_MENU_VERSION "${ATC_MENU_VERSION_FALLBACK}")
    if(NOT ATC_MENU_GIT_DESCRIBE)
        set(ATC_MENU_GIT_DESCRIBE "v${ATC_MENU_VERSION}-nogit")
    endif()
endif()

project(atc_menu VERSION ${ATC_MENU_VERSION} LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

#-----------------------------------------------------------------------------
# Configuration
#
# Two knobs, and both size buffers inside atc_menu_ctx_t — which is why they are
# recorded in build_config.h: an application that overrides one and links an
# artifact built with the other disagrees about the size of that struct. Row
# width and terminal height are not here on purpose; they come from
# atc_menu_screen_t at run time, so no build can differ from another over them.
#-----------------------------------------------------------------------------
set(ATC_MENU_MAX_DEPTH  6 CACHE STRING "Nested submenu depth")

configure_file(
    "${PROJECT_SOURCE_DIR}/include/atc_menu/version.h.in"
    "${PROJECT_BINARY_DIR}/generated/atc_menu/version.h" @ONLY)
configure_file(
    "${PROJECT_SOURCE_DIR}/include/atc_menu/build_config.h.in"
    "${PROJECT_BINARY_DIR}/generated/atc_menu/build_config.h" @ONLY)

# TI's cl430/cl2000 understand none of these, and a cross build must not fail
# over a diagnostic flag.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(ATC_MENU_WARN_FLAGS
        -Wall -Wextra -Werror -Wconversion -Wsign-conversion
        -Wshadow -Wcast-align -Wpointer-arith -Wstrict-prototypes)
else()
    set(ATC_MENU_WARN_FLAGS "")
endif()

#-----------------------------------------------------------------------------
# The library
#-----------------------------------------------------------------------------
# Carried separately from the archive so the test suite can compile the same
# source under sanitizers with the same configuration: a sanitizer only sees
# the code it instrumented, and linking the release archive would leave the
# code under test uninstrumented.
add_library(atc_menu_config INTERFACE)
target_include_directories(atc_menu_config INTERFACE
    include
    "${PROJECT_BINARY_DIR}/generated")

# An application compiling against this artifact must see the same values the
# archive was built with, or ATC_MENU_ASSERT_BUILD_MATCH is the only thing
# standing between it and a struct of the wrong size.
target_compile_definitions(atc_menu_config INTERFACE
    ATC_MENU_MAX_DEPTH=${ATC_MENU_MAX_DEPTH})

# Named once here because the test suite compiles the same list rather than
# linking the archive; see tests/CMakeLists.txt for why.
set(ATC_MENU_SOURCES
    "${PROJECT_SOURCE_DIR}/src/menu_buf.c"
    "${PROJECT_SOURCE_DIR}/src/menu_core.c"
    "${PROJECT_SOURCE_DIR}/src/menu_draw.c"
    "${PROJECT_SOURCE_DIR}/src/menu_edit.c"
    "${PROJECT_SOURCE_DIR}/src/menu_input.c"
    "${PROJECT_SOURCE_DIR}/src/menu_widget.c")

add_library(atc_menu STATIC ${ATC_MENU_SOURCES})

set_target_properties(atc_menu PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

target_link_libraries(atc_menu PUBLIC atc_menu_config)
target_compile_options(atc_menu PRIVATE ${ATC_MENU_WARN_FLAGS})

message(STATUS "atc_menu: ${ATC_MENU_GIT_DESCRIBE}, "
               "depth=${ATC_MENU_MAX_DEPTH}")

#-----------------------------------------------------------------------------
# Developer targets — off for a consumer that add_subdirectory()s this, and off
# for a cross build: both are host programs (stdio, a PC serial port, the
# filesystem) that a target toolchain would fail on long before the library.
#-----------------------------------------------------------------------------
if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_CROSSCOMPILING)
    set(_atc_menu_dev_default ON)
else()
    set(_atc_menu_dev_default OFF)
endif()

option(ATC_MENU_BUILD_EXAMPLES "Build the host demo" ${_atc_menu_dev_default})
option(ATC_MENU_BUILD_TESTS    "Build the test suite" ${_atc_menu_dev_default})

if(ATC_MENU_BUILD_EXAMPLES)
    # Host port used by the demo; not part of the library.
    add_library(atc_menu_port_host port/host/port_serial.c)
    target_include_directories(atc_menu_port_host PUBLIC port/host)
    add_subdirectory(examples/basic)
endif()

if(ATC_MENU_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

#-----------------------------------------------------------------------------
# Distribution: include/atc_menu + lib/libatc_menu.a
#-----------------------------------------------------------------------------
install(TARGETS atc_menu ARCHIVE DESTINATION lib)
install(FILES include/atc_menu/menu.h DESTINATION include/atc_menu)
install(FILES
    "${PROJECT_BINARY_DIR}/generated/atc_menu/version.h"
    "${PROJECT_BINARY_DIR}/generated/atc_menu/build_config.h"
    DESTINATION include/atc_menu)

# include(CPack) is process-wide, so only the top-level build packages —
# otherwise a consumer's own `cpack` would find it producing our tarball.
if(PROJECT_IS_TOP_LEVEL)
    set(CPACK_PACKAGE_NAME "atc_menu")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "atc_menu-${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Immediate-mode terminal menu for microcontrollers")
    set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
    set(CPACK_GENERATOR "TGZ")
    include(CPack)
endif()
