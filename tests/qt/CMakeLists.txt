# The Qt layer's view tests: the real widgets on Qt's offscreen platform (no display needed).
add_executable(Mandelbrotter_qt_tests
    FractalCanvasTests.cpp
    MainWindowTests.cpp
    platform_tests.cpp
    qt_test_main.cpp
    qt_util_tests.cpp)
target_include_directories(Mandelbrotter_qt_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
target_link_libraries(Mandelbrotter_qt_tests PRIVATE Mandelbrotter::qt Qt6::Test GTest::gtest)
set_target_properties(Mandelbrotter_qt_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_compile_definitions(Mandelbrotter_qt_tests PRIVATE
    MANDELBROTTER_HELP_DIR="${PROJECT_SOURCE_DIR}/docs/help"
    MANDELBROTTER_QT_OFFSCREEN_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/offscreen.json")
Mandelbrotter_configure_target(Mandelbrotter_qt_tests)

include(GoogleTest)
# PRE_TEST: the tests are listed when ctest runs them, with ctest's environment (not a test's ENVIRONMENT), which
# is why qt_test_main.cpp picks the offscreen platform itself.
gtest_discover_tests(Mandelbrotter_qt_tests DISCOVERY_MODE PRE_TEST TEST_FILTER "-FractionalRatio.*")

# The canvas at a fractional pixel ratio (Windows at 150%, fractional desktop scaling).
add_test(NAME qt.fractional_ratio COMMAND Mandelbrotter_qt_tests --gtest_filter=FractionalRatio.*)
set_tests_properties(qt.fractional_ratio PROPERTIES ENVIRONMENT "QT_SCALE_FACTOR=1.5")
