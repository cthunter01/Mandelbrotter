add_executable(Mandelbrotter_tests
    BigComplexTests.cpp
    BigFixedTests.cpp
    BlaTableTests.cpp
    bookmarks_tests.cpp
    cli_tests.cpp
    exporter_tests.cpp
    flights_tests.cpp
    fractal_tests.cpp
    help_action_tests.cpp
    help_book_tests.cpp
    help_images_tests.cpp
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
# The help book's sources, for the tests that check its pages, links and images (help_book_tests.cpp).
target_compile_definitions(Mandelbrotter_tests PRIVATE MANDELBROTTER_HELP_DIR="${PROJECT_SOURCE_DIR}/docs/help")
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

# The help book zip that gets embedded into the GUI (cmake/HelpBook.cmake) must carry the book at its root.
get_property(help_zip GLOBAL PROPERTY MANDELBROTTER_HELP_ZIP)
add_test(NAME help.book_zip
    COMMAND ${CMAKE_COMMAND} -DZIP=${help_zip} -P ${CMAKE_CURRENT_SOURCE_DIR}/help_zip_test.cmake)
