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

# Application layer: the window's logic without a toolkit (controllers, demo player, tour script, view-models).
# Links only the core, never wx, so the tests cover it and a second toolkit could sit on it.
add_library(Mandelbrotter_app STATIC
    app/AppController.cpp
    app/BookmarkStore.cpp
    app/CanvasController.cpp
    app/commands.cpp
    app/DemoPlayer.cpp
    app/ExportTask.cpp
    app/format.cpp
    app/help_routing.cpp
    app/help_sitemap.cpp
    app/panel_model.cpp
    app/scenes.cpp
    app/ScreenshotScript.cpp
    app/startup.cpp
    app/tour_layout.cpp
    app/TourScript.cpp
    app/ui_text.cpp)
add_library(Mandelbrotter::app ALIAS Mandelbrotter_app)
target_include_directories(Mandelbrotter_app PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(Mandelbrotter_app PUBLIC Mandelbrotter::lib)   # never wx
Mandelbrotter_configure_target(Mandelbrotter_app)

# GUI library: the wxWidgets layer. Headers live next to the sources and are included as "gui/<name>.h".
if(MANDELBROTTER_GUI STREQUAL "wx")
    add_library(Mandelbrotter_gui STATIC
        gui/app_icon.cpp
        gui/export_runner.cpp
        gui/ExportDialog.cpp
        gui/FractalCanvas.cpp
        gui/HelpController.cpp
        gui/JuliaPreview.cpp
        gui/MainFrame.cpp
        gui/MandelbrotterApp.cpp
        gui/ScreenshotRun.cpp
        gui/SidePanel.cpp
        gui/TourCard.cpp
        gui/TourView.cpp
        gui/window_capture.cpp
        gui/wx_util.cpp)
    add_library(Mandelbrotter::gui ALIAS Mandelbrotter_gui)
    target_include_directories(Mandelbrotter_gui PUBLIC ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(Mandelbrotter_gui PUBLIC Mandelbrotter::app Mandelbrotter::lib wx::html wx::core wx::base)
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
endif()

# GUI library: the Qt 6 Widgets layer, another thin view over the application layer. Headers live next to the
# sources and are included as "qt/<name>.h". No moc, uic or rcc automation (AUTOMOC and friends stay off).
if(MANDELBROTTER_GUI STREQUAL "qt")
    # The resources (the window icon) in a library of their own: Qt copies the owning target's compile options
    # into the object library it builds for them, and generated code must not meet our warnings (like stb_impl,
    # it is not passed through Mandelbrotter_configure_target). Every file gets an alias, so the paths inside
    # the resource never depend on where the sources live. Qt compiles a static library's resources into object
    # libraries it links into the executables, so the library itself only has an empty anchor source.
    file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/qt_resources_anchor.cpp
         CONTENT "// Generated by src/CMakeLists.txt: Mandelbrotter_qt_resources' only source of its own.\n")
    add_library(Mandelbrotter_qt_resources STATIC ${CMAKE_CURRENT_BINARY_DIR}/qt_resources_anchor.cpp)
    set(qt_resource_files ${CMAKE_CURRENT_SOURCE_DIR}/icons/mandelbrotter.png)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/icons/mandelbrotter.png
        PROPERTIES QT_RESOURCE_ALIAS icons/mandelbrotter.png)
    qt_add_resources(Mandelbrotter_qt_resources "mandelbrotter" PREFIX "/" BIG_RESOURCES
        FILES ${qt_resource_files})
    set_target_properties(Mandelbrotter_qt_resources PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

    add_library(Mandelbrotter_qt STATIC
        qt/ElidedLabel.cpp
        qt/MainWindow.cpp
        qt/platform.cpp
        qt/qt_util.cpp
        qt/run_qt.cpp)
    add_library(Mandelbrotter::qt ALIAS Mandelbrotter_qt)
    target_include_directories(Mandelbrotter_qt PUBLIC ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(Mandelbrotter_qt PUBLIC
        Mandelbrotter::app Mandelbrotter::lib Qt6::Widgets Qt6::PrintSupport Mandelbrotter_qt_resources)
    set_target_properties(Mandelbrotter_qt PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    # Strict Qt: no signal/slot keywords, no implicit conversions between QString and char*, and deprecations
    # capped at the minimum Qt, so a newer Qt cannot break -Werror with its own deprecations.
    target_compile_definitions(Mandelbrotter_qt
        PUBLIC  QT_NO_KEYWORDS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII QT_NO_NARROWING_CONVERSIONS_IN_CONNECT
                QT_DISABLE_DEPRECATED_UP_TO=0x060800 QT_WARN_DEPRECATED_UP_TO=0x060800
        PRIVATE MANDELBROTTER_VERSION="${PROJECT_VERSION}")
    Mandelbrotter_configure_target(Mandelbrotter_qt)
endif()

# The executable. main.cpp is free of any toolkit: it runs the command line (which never needs a display) and
# hands the window to runGui() (gui_entry.h), which the GUI build defines.
add_executable(Mandelbrotter main.cpp)
target_link_libraries(Mandelbrotter PRIVATE Mandelbrotter::app)
if(MANDELBROTTER_GUI STREQUAL "wx")
    # wxIMPLEMENT_APP_NO_MAIN must live in this target: in a static library the linker would drop the app
    # initializer object.
    target_sources(Mandelbrotter PRIVATE main_wx.cpp)
    target_link_libraries(Mandelbrotter PRIVATE Mandelbrotter::gui)
elseif(MANDELBROTTER_GUI STREQUAL "qt")
    target_link_libraries(Mandelbrotter PRIVATE Mandelbrotter::qt)   # runGui() is in src/qt/run_qt.cpp
else()
    target_sources(Mandelbrotter PRIVATE main_headless.cpp)
endif()
# Windows: the application icon (Explorer shows it for every build). For wx also wxWidgets' resources (wxMSW
# loads its stock cursors from the executable) and wx's per-monitor DPI manifest, which the linker merges into the
# one it writes so Windows stops bitmap-scaling the window.
if(WIN32)
    target_sources(Mandelbrotter PRIVATE Mandelbrotter.rc)
    set_source_files_properties(Mandelbrotter.rc PROPERTIES
        OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/icons/mandelbrotter.ico)
    if(MANDELBROTTER_GUI STREQUAL "wx")
        set_property(SOURCE Mandelbrotter.rc APPEND PROPERTY COMPILE_DEFINITIONS MANDELBROTTER_GUI_WX)
        target_sources(Mandelbrotter PRIVATE ${wxwidgets_SOURCE_DIR}/include/wx/msw/wx_dpi_aware_pmv2.manifest)
    endif()
endif()
# macOS: Mandelbrotter.app, so Finder opens it as an application (not in Terminal) and shows its icon. A build
# without a window stays a plain command-line executable.
if(APPLE AND NOT MANDELBROTTER_GUI STREQUAL "none")
    # Substituted into Mandelbrotter.plist.in along with the MACOSX_BUNDLE_* properties.
    if(MANDELBROTTER_GUI STREQUAL "wx")
        set(MANDELBROTTER_PRINCIPAL_CLASS wxNSApplication)
    else()
        set(MANDELBROTTER_PRINCIPAL_CLASS NSApplication)
    endif()
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
if(APPLE AND NOT MANDELBROTTER_GUI STREQUAL "none")
    install(CODE [[
        execute_process(COMMAND codesign --force --sign - "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Mandelbrotter.app"
                        COMMAND_ERROR_IS_FATAL ANY)
    ]])
endif()
# Linux desktops find an application's name and icon through a .desktop file named after its window class
# (the Wayland app_id), "Mandelbrotter". A build without a window has no menu entry.
if(UNIX AND NOT APPLE AND NOT MANDELBROTTER_GUI STREQUAL "none")
    install(FILES Mandelbrotter.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES icons/mandelbrotter.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
endif()
