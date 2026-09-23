# Core library: all the maths and rendering, free of any GUI dependency, so the tests and the CLI can use it.
add_library(Mandelbrotter_lib STATIC
    core/BigComplex.cpp
    core/BigFixed.cpp
    core/BlaTable.cpp
    core/bookmarks.cpp
    core/cli.cpp
    core/exporter.cpp
    core/flights.cpp
    core/fractal.cpp
    core/help_action.cpp
    core/help_images.cpp
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

# Build-time tool: turns a file into a C++ source (the help book, see cmake/HelpBook.cmake). Never installed.
add_executable(Mandelbrotter_embed tools/embed_file.cpp)
Mandelbrotter_configure_target(Mandelbrotter_embed)

# GUI library: the wxWidgets layer. Headers live next to the sources and are included as "gui/<name>.h".
add_library(Mandelbrotter_gui STATIC
    gui/BookmarkStore.cpp
    gui/DemoPlayer.cpp
    gui/export_runner.cpp
    gui/ExportDialog.cpp
    gui/FractalCanvas.cpp
    gui/GuidedTour.cpp
    gui/HelpController.cpp
    gui/JuliaPreview.cpp
    gui/MainFrame.cpp
    gui/MandelbrotterApp.cpp
    gui/ScreenshotRun.cpp
    gui/SidePanel.cpp
    gui/TourCard.cpp
    gui/window_capture.cpp
    gui/wx_util.cpp)
add_library(Mandelbrotter::gui ALIAS Mandelbrotter_gui)
target_include_directories(Mandelbrotter_gui PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(Mandelbrotter_gui PUBLIC Mandelbrotter::lib wx::html wx::core wx::base)
# window_capture.cpp reads a window's pixels back through GDK on Linux; wx keeps its GTK include directories
# private, so the file needs them itself (an imported target: the headers count as system headers).
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
    target_link_libraries(Mandelbrotter_gui PRIVATE PkgConfig::GTK3)
endif()
target_compile_definitions(Mandelbrotter_gui PRIVATE MANDELBROTTER_VERSION="${PROJECT_VERSION}")
include(${PROJECT_SOURCE_DIR}/cmake/HelpBook.cmake)
Mandelbrotter_add_help_book(Mandelbrotter_gui)
Mandelbrotter_configure_target(Mandelbrotter_gui)

# The executable. wxIMPLEMENT_APP_NO_MAIN must live in this target: in a static library the linker would drop
# the app initializer object.
add_executable(Mandelbrotter main.cpp)
target_link_libraries(Mandelbrotter PRIVATE Mandelbrotter::gui)
Mandelbrotter_configure_target(Mandelbrotter)

include(GNUInstallDirs)
install(TARGETS Mandelbrotter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
