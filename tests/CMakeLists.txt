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
