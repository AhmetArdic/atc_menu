cmake_minimum_required(VERSION 3.15)
project(atc_menu C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ATC_MENU_BUILD_TESTS    "Build unit tests"  ON)
option(ATC_MENU_BUILD_EXAMPLES "Build example app" OFF)

# ---------------------------------------------------------------------------
# Core library
# ---------------------------------------------------------------------------
add_library(atc_menu STATIC
    src/menu.c
    src/render.c
    src/nav.c
    src/fmt.c
)

target_include_directories(atc_menu
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(atc_menu PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

# Layout tunables — pass via cmake -DLABEL_W=32 etc. See src/layout.h.
foreach(_param LABEL_W VALUE_W UNIT_W NAV_DEPTH)
    if(DEFINED ${_param})
        target_compile_definitions(atc_menu PUBLIC ${_param}=${${_param}})
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Mock port
# ---------------------------------------------------------------------------
add_library(atc_menu_mock STATIC
    ports/mock/mock_port.c
)

target_include_directories(atc_menu_mock
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ports/mock
)

target_link_libraries(atc_menu_mock PUBLIC atc_menu)

# ---------------------------------------------------------------------------
# Tests / examples
# ---------------------------------------------------------------------------
if(ATC_MENU_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ATC_MENU_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
