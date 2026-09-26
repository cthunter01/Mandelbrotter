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
    gui/app_icon.cpp
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

    # The window icon (gui/app_icon.cpp), compiled in so the executable needs no files beside it.
    set(icon_png ${CMAKE_CURRENT_SOURCE_DIR}/icons/mandelbrotter.png)
    set(icon_cpp ${PROJECT_BINARY_DIR}/icons/app_icon_data.cpp)
    add_custom_command(OUTPUT ${icon_cpp}
        COMMAND Mandelbrotter_embed ${icon_png} ${icon_cpp} gui/app_icon.h mandelbrotter::gui appIconPngChunks
        DEPENDS ${icon_png} Mandelbrotter_embed
        COMMENT "Embedding the application icon"
        VERBATIM)
    set_source_files_properties(${icon_cpp} PROPERTIES GENERATED TRUE SKIP_LINTING ON)
    target_sources(Mandelbrotter_gui PRIVATE ${icon_cpp})
endif()
target_compile_definitions(Mandelbrotter_gui PRIVATE MANDELBROTTER_VERSION="${PROJECT_VERSION}")
include(${PROJECT_SOURCE_DIR}/cmake/HelpBook.cmake)
Mandelbrotter_add_help_book(Mandelbrotter_gui)
Mandelbrotter_configure_target(Mandelbrotter_gui)

# The executable. wxIMPLEMENT_APP_NO_MAIN must live in this target: in a static library the linker would drop
# the app initializer object.
add_executable(Mandelbrotter main.cpp)
target_link_libraries(Mandelbrotter PRIVATE Mandelbrotter::gui)
# Windows: the application icon and wxWidgets' resources (wxMSW loads its stock cursors from the executable),
# and wx's per-monitor DPI manifest, which the linker merges into the one it writes so Windows stops
# bitmap-scaling the window.
if(WIN32)
    target_sources(Mandelbrotter PRIVATE
        Mandelbrotter.rc
        ${wxwidgets_SOURCE_DIR}/include/wx/msw/wx_dpi_aware_pmv2.manifest)
    set_source_files_properties(Mandelbrotter.rc PROPERTIES
        OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/icons/mandelbrotter.ico)
endif()
# macOS: Mandelbrotter.app, so Finder opens it as an application (not in Terminal) and shows its icon.
if(APPLE)
    set_target_properties(Mandelbrotter PROPERTIES
        MACOSX_BUNDLE                      TRUE
        MACOSX_BUNDLE_INFO_PLIST           ${CMAKE_CURRENT_SOURCE_DIR}/Mandelbrotter.plist.in
        MACOSX_BUNDLE_BUNDLE_NAME          Mandelbrotter
        MACOSX_BUNDLE_GUI_IDENTIFIER       io.github.cthunter01.Mandelbrotter
        MACOSX_BUNDLE_ICON_FILE            mandelbrotter.icns
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_BUNDLE_VERSION       ${PROJECT_VERSION})
    target_sources(Mandelbrotter PRIVATE icons/mandelbrotter.icns)
    set_source_files_properties(icons/mandelbrotter.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()
Mandelbrotter_configure_target(Mandelbrotter)

include(GNUInstallDirs)
install(TARGETS Mandelbrotter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} BUNDLE DESTINATION .)
# The linker signs only the executable inside the bundle; sign the installed bundle as a whole (ad hoc, no
# identity), so its Info.plist and icon are sealed too and `codesign --verify` accepts it.
if(APPLE)
    install(CODE [[
        execute_process(COMMAND codesign --force --sign - "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Mandelbrotter.app"
                        COMMAND_ERROR_IS_FATAL ANY)
    ]])
endif()
# Linux desktops find an application's name and icon through a .desktop file named after its window class
# (the Wayland app_id), "Mandelbrotter".
if(UNIX AND NOT APPLE)
    install(FILES Mandelbrotter.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES icons/mandelbrotter.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
endif()
