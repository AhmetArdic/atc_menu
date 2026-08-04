cmake_minimum_required(VERSION 3.16)
project(atc_menu C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(ATC_MENU_SOURCES
    src/menu_core.c
    src/menu_render.c
    src/menu_input.c
    src/menu_fmt.c
)

add_library(atc_menu STATIC ${ATC_MENU_SOURCES})
target_include_directories(atc_menu PUBLIC include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(atc_menu PRIVATE -Wall -Wextra)
endif()

add_executable(menu_demo
    examples/basic/main.c
    port/host/port_stdio.c
    port/host/port_serial.c
)
target_include_directories(menu_demo PRIVATE port/host)
target_link_libraries(menu_demo PRIVATE atc_menu)

enable_testing()

# The goldens and the wrap assertions are written for a 40-column row, so the
# suite builds its own copy of the library pinned to that width instead of
# following ATC_MENU_CFG_LABEL's default. The width has to be pinned for the
# library too, not just the test: it sits in atc_menu_ctx_t.
add_library(atc_menu_pinned STATIC ${ATC_MENU_SOURCES})
target_include_directories(atc_menu_pinned PUBLIC include)
target_compile_definitions(atc_menu_pinned PUBLIC ATC_MENU_CFG_LABEL=18)

add_executable(menu_tests test/test_main.c)
target_include_directories(menu_tests PRIVATE src)
target_link_libraries(menu_tests PRIVATE atc_menu_pinned)
add_test(
    NAME menu_tests
    COMMAND menu_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
)

# Regenerates test/golden/*.bin after an intentional rendering change.
add_custom_target(update_golden
    COMMAND menu_tests --update
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
    DEPENDS menu_tests
)
