add_library(testing INTERFACE)
target_include_directories(testing INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

function(atc_add_test name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE atc_menu_mock testing)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

atc_add_test(test_init)
atc_add_test(test_printf)
atc_add_test(test_handle_key)
atc_add_test(test_render)
atc_add_test(test_header)
atc_add_test(test_widgets)

# Visual-inspection dumps (not CTest targets — run by hand to eyeball output).
add_executable(dump_render dump_render.c)
target_link_libraries(dump_render PRIVATE atc_menu_mock)

add_executable(dump_warn dump_warn.c)
target_link_libraries(dump_warn PRIVATE atc_menu_mock)
