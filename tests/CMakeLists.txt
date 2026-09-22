add_executable(Mandelbrotter_tests
    bookmarks_test.cpp
    cli_test.cpp
    exporter_test.cpp
    fractal_test.cpp
    image_test.cpp
    kernel_test.cpp
    palette_test.cpp
    png_writer_test.cpp
    render_settings_test.cpp
    renderer_test.cpp
    viewport_test.cpp)
target_link_libraries(Mandelbrotter_tests PRIVATE Mandelbrotter::lib GTest::gtest_main stb::stb)
Mandelbrotter_configure_target(Mandelbrotter_tests)

include(GoogleTest)
# PRE_TEST: list the tests when ctest runs, not at build time (the binary may be sanitized or cross-compiled).
gtest_discover_tests(Mandelbrotter_tests DISCOVERY_MODE PRE_TEST)

# Integration test: the real executable renders headlessly. DISPLAY is cleared to prove the CLI path never needs
# the GUI toolkit.
add_test(NAME cli.render_png
    COMMAND ${CMAKE_COMMAND}
        -DEXE=$<TARGET_FILE:Mandelbrotter>
        -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_render_test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_render_test.cmake)
set_tests_properties(cli.render_png PROPERTIES ENVIRONMENT "DISPLAY=;WAYLAND_DISPLAY=" TIMEOUT 120)
