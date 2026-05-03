cmake_minimum_required(VERSION 3.15)
project(atc_menu C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ATC_MENU_BUILD_TESTS    "Build unit tests"  ON)
option(ATC_MENU_BUILD_EXAMPLES "Build example app" OFF)

option(ATC_MENU_WIDGET_BAR    "Enable BAR widget"    ON)
option(ATC_MENU_WIDGET_CHOICE "Enable CHOICE widget" ON)
option(ATC_MENU_WIDGET_INPUT  "Enable INPUT widget"  ON)
option(ATC_MENU_INPUT_FLOAT   "Enable FLOAT input parser (pulls strtod)" ON)

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
)

if(ATC_MENU_WIDGET_BAR)
    target_sources(atc_menu PRIVATE src/widgets/widget_bar.c)
endif()
if(ATC_MENU_WIDGET_CHOICE)
    target_sources(atc_menu PRIVATE src/widgets/widget_choice.c)
endif()
if(ATC_MENU_WIDGET_INPUT)
    target_sources(atc_menu PRIVATE src/widgets/widget_input.c)
endif()

target_include_directories(atc_menu
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(atc_menu PUBLIC
    ATC_MENU_WIDGET_BAR=$<BOOL:${ATC_MENU_WIDGET_BAR}>
    ATC_MENU_WIDGET_CHOICE=$<BOOL:${ATC_MENU_WIDGET_CHOICE}>
    ATC_MENU_WIDGET_INPUT=$<BOOL:${ATC_MENU_WIDGET_INPUT}>
    ATC_MENU_INPUT_FLOAT=$<BOOL:${ATC_MENU_INPUT_FLOAT}>
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
