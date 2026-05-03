cmake_minimum_required(VERSION 3.15)
project(atc_menu C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ATC_MENU_BUILD_TESTS    "Build unit tests"  ON)
option(ATC_MENU_BUILD_EXAMPLES "Build example app" OFF)

# ---------------------------------------------------------------------------
# Core library
# ---------------------------------------------------------------------------
add_library(atc_menu STATIC
    src/core/menu.c
    src/render/render.c
    src/input/nav.c
    src/input/cmdmode.c
    src/widgets/widget.c
    src/widgets/widget_group.c
    src/widgets/widget_submenu.c
    src/widgets/widget_value.c
    src/widgets/widget_state.c
    src/widgets/widget_action.c
    src/widgets/widget_bar.c
    src/widgets/widget_choice.c
    src/widgets/widget_input.c
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

# ---------------------------------------------------------------------------
# Mock port (only needed when tests are enabled, but cheap to expose)
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
