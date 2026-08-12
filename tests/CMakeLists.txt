# The library source is compiled into the test binary rather than linked from
# the archive: a sanitizer only reports on the code it instrumented, and the
# library is the code under test.
add_executable(test_menu
    test_menu.c
    fake_sink.c
    ${ATC_MENU_SOURCES})

target_link_libraries(test_menu PRIVATE atc_menu_config)
target_compile_options(test_menu PRIVATE -g -O1 -fsanitize=address,undefined)
target_link_options(test_menu PRIVATE -fsanitize=address,undefined)

add_test(NAME menu COMMAND test_menu)
