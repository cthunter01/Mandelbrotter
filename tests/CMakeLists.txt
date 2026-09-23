add_executable(Mandelbrotter_tests
    BigComplexTests.cpp
    BigFixedTests.cpp
    BlaTableTests.cpp
    bookmarks_tests.cpp
    cli_tests.cpp
    exporter_tests.cpp
    fractal_tests.cpp
    image_tests.cpp
    kernel_tests.cpp
    PaletteTests.cpp
    perturbation_tests.cpp
    png_writer_tests.cpp
    ProgressiveRendererTests.cpp
    ReferenceOrbitTests.cpp
    RenderSettingsTests.cpp
    ViewportTests.cpp)
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
