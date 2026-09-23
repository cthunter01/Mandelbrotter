# Core library: all the maths and rendering, free of any GUI dependency, so the tests and the CLI can use it.
add_library(Mandelbrotter_lib STATIC
    core/BigComplex.cpp
    core/BigFixed.cpp
    core/BlaTable.cpp
    core/bookmarks.cpp
    core/cli.cpp
    core/exporter.cpp
    core/fractal.cpp
    core/image.cpp
    core/kernel.cpp
    core/Palette.cpp
    core/perturbation.cpp
    core/png_writer.cpp
    core/ProgressiveRenderer.cpp
    core/ReferenceOrbit.cpp
    core/RenderSettings.cpp
    core/Viewport.cpp)
add_library(Mandelbrotter::lib ALIAS Mandelbrotter_lib)
target_include_directories(Mandelbrotter_lib PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(Mandelbrotter_lib
    PUBLIC  Threads::Threads
    PRIVATE nlohmann_json::nlohmann_json stb::stb)   # implementation details, never in public headers
Mandelbrotter_configure_target(Mandelbrotter_lib)

# GUI library: the wxWidgets layer. Headers live next to the sources and are included as "gui/<name>.h".
add_library(Mandelbrotter_gui STATIC
    gui/BookmarkStore.cpp
    gui/export_runner.cpp
    gui/ExportDialog.cpp
    gui/FractalCanvas.cpp
    gui/JuliaPreview.cpp
    gui/MainFrame.cpp
    gui/MandelbrotterApp.cpp
    gui/SidePanel.cpp
    gui/wx_util.cpp)
add_library(Mandelbrotter::gui ALIAS Mandelbrotter_gui)
target_include_directories(Mandelbrotter_gui PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(Mandelbrotter_gui PUBLIC Mandelbrotter::lib wx::core wx::base)
Mandelbrotter_configure_target(Mandelbrotter_gui)

# The executable. wxIMPLEMENT_APP_NO_MAIN must live in this target: in a static library the linker would drop
# the app initializer object.
add_executable(Mandelbrotter main.cpp)
target_link_libraries(Mandelbrotter PRIVATE Mandelbrotter::gui)
Mandelbrotter_configure_target(Mandelbrotter)

include(GNUInstallDirs)
install(TARGETS Mandelbrotter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
