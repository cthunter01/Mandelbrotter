# Mandelbrotter

Interactive Mandelbrot-family fractal explorer. C++23 project built with CMake presets + Ninja, wxWidgets 3.2 GUI
(statically linked, built in-tree via FetchContent), tested with GoogleTest. Cross-platform: Linux (GCC, Clang),
macOS (Apple Clang), Windows (MSVC) and FreeBSD (Clang). A second front end in Qt 6 Widgets (an installed Qt 6.8+)
duplicates the wx GUI; `MANDELBROTTER_GUI` (`wx`, the default, `qt` or `none`) picks one per build tree, and
releases are wx only.

## Commands
- Build and test (Clang Debug): `cmake --workflow --preset dev`
- Rebuild only: `cmake --build --preset clang-debug`
- Test only: `ctest --preset clang-debug`
- One test: `ctest --preset clang-debug -R 'Kernel\.'` or `build/clang-debug/bin/Mandelbrotter_tests --gtest_filter='Kernel.*'`
- Run the app: `build/clang-debug/bin/Mandelbrotter` (use `clang-release` for interactive speed; on macOS it is
  a bundle, `open build/clang-debug/bin/Mandelbrotter.app`); headless render:
  `build/clang-debug/bin/Mandelbrotter --render out.png --size 640x400`
- Before finishing a change, also run: `cmake --workflow --preset tidy` (clang-tidy, warnings are errors) and
  `cmake --workflow --preset asan` (AddressSanitizer + UBSan); `cmake --workflow --preset tsan` for renderer changes
- The Qt front end (`MANDELBROTTER_GUI=qt`, needs Qt 6.8+ installed; `CMAKE_PREFIX_PATH` where it is not on the
  default search path): `cmake --workflow --preset dev-qt` (Windows: `dev-msvc-qt`), and `tidy-qt` and `asan-qt`
  before finishing a change to it; its view tests run on Qt's offscreen platform (no display needed). A change to
  the app layer or to shared texts touches both front ends: run `dev` and `dev-qt`. `headless` builds without any
  GUI (wxWidgets is not fetched)
- On Windows the presets are `msvc-debug` (workflow `dev-msvc`), `msvc-release` and `ci-msvc`, and cmake must run
  in a Developer PowerShell for VS. `tidy`, `asan`, `tsan` and `coverage` exist on Linux and macOS only
- Formatting is automatic: a Claude Code hook (`.claude/hooks/format-cpp.sh`) runs clang-format on every C/C++
  file right after you edit it. The pre-commit hook and CI also reject unformatted files
- Help book images: after a change to the window or to `helpImageSpecs()`, regenerate them with a release build on
  a Linux desktop: `GDK_BACKEND=x11 build/clang-release/bin/Mandelbrotter --screenshots docs/help/images` (twice
  when the rendered pictures changed, so the help-window screenshot shows them); the Qt build's own screenshots
  with `QT_QPA_PLATFORM=offscreen:configfile=tests/qt/offscreen.json build/clang-release-qt/bin/Mandelbrotter
  --screenshots docs/help/images-qt` (twice, rebuilding in between, so its help-window shot shows them); `ctest
  --preset clang-debug -R HelpBook` checks the book
- Application icon: `src/icons/make_icons.sh` renders it with a release build and rewrites `src/icons/` (needs
  ImageMagick 7, and Python 3 with Pillow for the `.icns`)
- README pictures: `docs/readme/make_media.sh` renders the banner and the demo GIFs with a release build (needs
  ImageMagick 7 and ffmpeg; a few minutes). The GIF frames come from the hidden developer option
  `--flight ID --flight-frames DIR [--fps N]`, which renders every frame of a demo flight to completion; after a
  change to a flight in `flights.h`, regenerate them

Other presets: `clang-release`, `gcc-debug`, `gcc-release`, `tsan`, `coverage`, `ci-gcc`, `ci-clang`, `headless`,
`dist-linux`, `dist-macos`, `dist-windows` (release archives, in `build/dist-<os>/package/`), and the Qt builds
`clang-debug-qt`, `clang-release-qt`, `gcc-debug-qt`, `msvc-debug-qt`, `msvc-release-qt`, `ci-gcc-qt`,
`ci-clang-qt`, `ci-msvc-qt`, `asan-qt`, `tidy-qt` (`tsan` and `coverage` stay wx only)
(`gcc-debug` and the like are configure/build/test presets, not workflows).
Each builds into `build/<preset>/`; never edit anything under `build/`. The first configure of a preset
downloads and compiles wxWidgets (minutes); ccache makes later presets fast. A preset is only available on the
platforms it supports (`gcc-*`: Linux; `clang-*`: Linux, macOS and FreeBSD; `msvc-*`: Windows);
`cmake --list-presets` shows this machine's.

Releases: the `Release` GitHub workflow (`.github/workflows/release.yml`) runs only when started by hand. It
tags `v<project VERSION>` and publishes the `dist-*` archives (wx; the Linux one still needs system GTK 3 at
runtime; macOS and Windows are self-contained), so raise `VERSION` in `project()` in `CMakeLists.txt` first. It
runs `ci.yml` (the wx and `headless` builds) first; the Qt builds have their own workflow, `ci-qt.yml`, which
does not gate a release.

## Layout
- `include/Mandelbrotter/`: public headers of the core library (and, in `app/`, of the application layer)
- `src/core/`: `Mandelbrotter_lib` — kernels, viewport, palettes, progressive renderer, exporter, PNG writer,
  bookmarks (JSON), CLI, and the deep-zoom machinery: `BigFixed`/`BigComplex` (fixed-point big numbers),
  `ReferenceOrbit` and `perturbation.h`. **No wxWidgets here**, ever: the tests and the `--render` path must
  work headless
- `include/Mandelbrotter/app/` + `src/app/`: `Mandelbrotter_app` — the window's logic without a toolkit, linking
  only the core and **never wx**, unit-tested. `AppController` (the model, bookmarks, status texts, demos and the
  snapshot, help-action dispatch; a toolkit fills its `Shell` of hooks), `CanvasController` (render bookkeeping,
  navigation, the orbit overlay, all in logical pixels), `DemoPlayer` (plays a core `Flight` on the toolkit's
  timer), `TourScript` (the tour's eleven steps and state), `ExportTask` (the PNG export's worker thread),
  `BookmarkStore`, `ScreenshotScript` (the screenshot mode's shots and state machine; a toolkit captures), and
  the helpers `commands.h` (the menu bar as data; `AppController::runCommand` runs the commands that need no
  toolkit), `ui_text.h` (every text and size the windows show), `help_sitemap.h` (the help book's control files,
  page text and full-text search, for a help viewer of our own), `format.h`, `scenes.h` (the places the tour and
  screenshots show), `startup.h` (`prepareStartup`: the command line before any window), `panel_model.h`,
  `help_routing.h` and `tour_layout.h`
- `src/gui/`: `Mandelbrotter_gui` — the wxWidgets layer, a thin view over the app layer; headers live beside
  sources, included as `"gui/x.h"`. `MainFrame` implements `AppController::Shell`, `FractalCanvas` paints what
  its `CanvasController` holds and translates events. Besides the frame, canvas, panel and dialogs:
  `HelpController` (the wxHTML help window over the embedded book), `TourView` + `TourCard` (the tour on screen;
  the card is a child panel of the frame, never a top-level window, because Wayland does not let apps position
  those), `ScreenshotRun` (fills `ScreenshotScript`'s hooks for the `--screenshots DIR` developer mode), and the
  two files with per-platform code: `window_capture.cpp` and
  `app_icon.cpp` (the window icon: from the executable's resources on Windows, from the embedded PNG on GTK,
  nothing on macOS, which shows only an .app bundle's icon)
- `src/qt/`: `Mandelbrotter_qt` — the Qt 6 Widgets layer, another thin view over the app layer (built only for
  `MANDELBROTTER_GUI=qt`); headers beside sources, included as `"qt/x.h"`. `MainWindow` (a `QMainWindow`: the
  `Shell`, menus from `menuBar()`, a status bar of `ElidedLabel`s, the dialogs' lifecycles, F1 routing),
  `FractalCanvas` (works in device pixels at a fractional pixel ratio, as wxMSW does), `SidePanel` in a dock,
  `JuliaPreview`, `GlobalKeyFilter` (the main window's keys before its controls: Esc stops a demo, any key ends a
  flight, menu shortcuts beat text fields), `ExportDialog` + `export_runner`, `HelpWindow` (a `QTextBrowser` over
  the book embedded as resources, with contents, index and search from `help_sitemap.h`), `TourView` +
  `TourCard`, `ScreenshotRun` (over `QWidget::grab()`, which also works offscreen and on Wayland), `run_qt.cpp`
  (`runGui`) and `platform.cpp`, the layer's only per-platform file (`userDataDir()`: the same bookmarks
  directory as wx, `~/.Mandelbrotter` on Linux and FreeBSD). The resources (book and icon) are a library of their
  own, `Mandelbrotter_qt_resources`, so Qt's generated code never meets our warnings
- `src/tools/`: build-time tools; `embed_file.cpp` (`Mandelbrotter_embed`) turns the zipped help book into a C++
  source of string-literal chunks (`cmake/HelpBook.cmake`)
- `docs/help/`: the help book: `help.hhp`, `contents.hhc`, `index.hhk`, hand-written pages in wxHTML (ASCII, no
  CSS) and `images/` (committed; `ui-*.png` are the wx build's screenshots, the rest rendered examples from
  `helpImageSpecs()`); `images-qt/` holds the Qt build's screenshots under the same names. Embedded at build time
  (wx: zipped, Qt: resources, each with its own screenshots); `tests/help_book_tests.cpp` checks every link, image
  and action link, and the image budget per build. Authoring notes in `docs/help/README.md`
- `src/main.cpp`: the executable's `main()`, free of any toolkit: `app::prepareStartup()` runs the command line,
  `runGui()` (`src/gui_entry.h`) opens the window. The wx build defines it in `src/main_wx.cpp`, where
  `wxIMPLEMENT_APP_NO_MAIN` must stay (in a static library the linker drops the app initializer), the Qt build in
  `src/qt/run_qt.cpp`, a build without a GUI in `src/main_headless.cpp`. Beside it, each platform's wrapping:
  `Mandelbrotter.rc` (Windows: the icon, and wxWidgets' resources, from which wxMSW loads stock cursors such as
  `wxCURSOR_BULLSEYE`), `Mandelbrotter.plist.in` (macOS: the executable is `Mandelbrotter.app`, signed ad hoc when
  installed) and `Mandelbrotter.desktop` (Linux: installed with the icon, named after the window class)
- `src/icons/`: the application icon (`.ico` for Windows, `.icns` for the macOS bundle, `.png` embedded on Linux
  and installed for the `.desktop` file), generated by `make_icons.sh`
- `docs/readme/`: the README's banner and demo GIFs, generated by `make_media.sh`
- `tests/`: GoogleTest files, all in `Mandelbrotter_tests` (class tests: `MyClassTests.cpp`; other tests:
  `*_tests.cpp`); `cli_render_test.cmake` runs the real executable with `DISPLAY` cleared. The app layer is tested
  through `AppHarness.h` (a recording `Shell` and a fake clock) and `PostQueue.h` (the toolkit's event queue:
  renders post there and nothing runs until the test drains it). `tests/qt/`: `Mandelbrotter_qt_tests`, the Qt
  view tests on the offscreen platform (`qt_test_main.cpp` picks it, with `offscreen.json`'s large screen);
  `QtHarness.h` (`pumpUntil`, `activate`, a window with its bookmarks in a temp dir), `MainWindow::dialogSeams`
  answer the modal dialogs; the screenshot run is labeled `slow`, and `qt.fractional_ratio` runs the canvas at
  `QT_SCALE_FACTOR=1.5`
- `cmake/ProjectOptions.cmake`: `Mandelbrotter_configure_target()` (warnings, sanitizers, coverage, tidy)
- `cmake/Dependencies.cmake`: third-party libraries via FetchContent (GoogleTest, sanitized like our targets when
  built from source; nlohmann/json, stb, and for `MANDELBROTTER_GUI=wx` wxWidgets with its option block: static,
  native toolkit, unneeded components off except HTML and the help subsystem for the in-app help, no
  `find_package` fallback because a shared system wx would otherwise be picked up); for `qt`, `find_package(Qt6
  6.8)`

## Conventions
- Headers are `.h` (never `.hpp`) and use `#pragma once`
- A class's header and implementation files are named exactly after the class, including capitalization:
  `class MyClass` lives in `include/Mandelbrotter/MyClass.h` and `src/core/MyClass.cpp` (app classes:
  `include/Mandelbrotter/app/MyClass.h` and `src/app/MyClass.cpp`; GUI classes: `src/gui/MyClass.h` and `.cpp`, or
  `src/qt/MyClass.h` and `.cpp`), and
  its tests in `tests/MyClassTests.cpp`. Headers of free functions or of several small types keep
  snake_case names (`kernel.h`, `geometry.h`), with tests named `*_tests.cpp`
- Code lives in `namespace mandelbrotter` (app layer: `mandelbrotter::app`; GUI: `mandelbrotter::gui`, Qt:
  `mandelbrotter::qt`); project
  includes use quotes: `#include "Mandelbrotter/kernel.h"`, `#include "Mandelbrotter/app/AppController.h"`
- Application behavior goes into `src/app` with a test, not into a view class (wx or Qt): a view translates its
  events into controller calls and implements the controller's hooks (`std::function` structs, not interfaces).
  The numbers the help book documents (drag threshold, zoom steps, timers, preview size) are `inline constexpr` in
  the app headers. Menus come from `commands.h` and user-visible texts and sizes from `ui_text.h`, never literals
  in a view, so both front ends say what the help book says. `src/app` gets the root `.clang-tidy` unrelaxed
- Every new target must call `Mandelbrotter_configure_target(<target>)`
- New source files go into the relevant `CMakeLists.txt`; new tests go into `tests/CMakeLists.txt`
- Warnings are part of the build: code must compile cleanly with `-Werror` under GCC and Clang and with `/WX`
  under MSVC, and pass clang-tidy on Linux (`.clang-tidy`; `src/gui/.clang-tidy`, `src/qt/.clang-tidy`,
  `tests/.clang-tidy` and `tests/qt/.clang-tidy` relax a few checks for wx, Qt and gtest)
- Code must build and pass its tests on Linux, macOS, Windows and FreeBSD (CI runs all four). Use the standard
  library (`<filesystem>`, `<thread>`, `<chrono>`) over POSIX or Win32 APIs; when an OS API is unavoidable, keep it
  in one source file behind an `#ifdef _WIN32` / `__APPLE__` / `__linux__` / `__FreeBSD__` split, with a branch for
  each platform (Linux and FreeBSD may share one: both are GTK 3 for wx, `__WXGTK__`, and X11 or Wayland for Qt)
- FreeBSD is built with the base system's Clang and libc++, and CI runs 14.5 (Clang 21): FreeBSD 15.0 and 15.1
  ship libc++ 19, which lacks `std::from_chars` for floating point and has `std::jthread` only behind
  `-fexperimental-library`. Leave the jobs at `release: "14.5"` until a 15.x release has libc++ 20 or later
- Deep zoom: `ViewSpec::center` is a `BigComplex` whose precision follows the zoom (`fractionBitsFor`);
  kernels never see absolute big positions, only double offsets from the center (`Viewport::offsetFromCenter`).
  Above `kPerturbationZoom` (1e8) the renderer iterates every pixel as a delta from the center's
  `ReferenceOrbit` (`iteratePerturbed`, with rebasing) and jumps stretches where the delta is tiny through the
  orbit's `BlaTable` (bilinear approximation; `kBlaEpsilon` trades speed for the last digits); below it the
  direct double kernel runs. Bookmarks and
  view files store centers as decimal strings (schema version 2); `BigFixed::fromDecimal` rounds to nearest
  and `toDecimal` prints enough digits that a saved view reloads bit-for-bit
- Renderer callbacks run on worker threads: `CanvasController` hands them to its `post` hook, which the toolkit
  runs on its own thread (wx: `CallAfter`; Qt: a queued `QMetaObject::invokeMethod` on the canvas); never touch
  toolkit objects or controller state from a worker
- Help actions: links of the form `mandelbrotter:<verb> ...` in the book (`help_action.h`: `view <CLI options>`,
  `flight <id>`, `tour`, `orbit on|off`, `reset`, `export`) are executed by `AppController::runHelpAction`; `view`
  reuses the CLI parser, so the words are the same as on the command line. Demo flights are data in
  `flights.h` (`builtinFlights()`), interpolated by pure functions; the Demos menu, the demos page and the
  `place-*` images all derive from them. The Save image dialog is modeless (the tour and the screenshot mode
  drive it); `AppController`'s public methods are the automation surface the help system uses (`frame.app()`),
  and `MainFrame` / `MainWindow` keep only what needs their toolkit (menus, dialogs, clipboard, the status bar
  and panel widgets)
- wxWidgets rules: `Bind()` only (no event tables, no `wxIMPLEMENT_DYNAMIC_CLASS`); convert text with
  `toWx()`/`fromWx()` from `gui/wx_util.h` (UTF-8), never `wxString(std::string)`; keep string literals given
  to wx ASCII-only (a non-ASCII narrow literal becomes an empty `wxString` under the C locale); no
  `wxString::Format`/`wxLogXXX` varargs (use `std::format` + `toWx`). Sizes, borders and pen widths in pixels go
  through `FromDIP()`: the Windows executable is per-monitor DPI aware, so wxMSW does not scale raw pixel values
  (GTK and macOS scale them already)
- Qt rules (`src/qt/`, namespace `mandelbrotter::qt`, headers beside sources included as `"qt/x.h"`, a class in
  `src/qt/MyClass.h`/`.cpp`): Qt Widgets, no QML; no `Q_OBJECT`, no signals or slots of our own, no moc, uic or
  rcc automation (`AUTOMOC`/`AUTOUIC`/`AUTORCC` off on our targets); `connect()` only with lambdas or Qt's
  member-function pointers and always with a context object; callbacks between our classes are `std::function`
  members. Text crosses the boundary only through `toQt()`/`fromQt()` from `qt/qt_util.h` (UTF-8); literals come
  from `app/ui_text.h` or `commands.h`, or `u"..."_s` when Qt-only. Programmatic updates of controls run under
  `QSignalBlocker` (Qt, unlike wx, signals them). A widget closed from inside its own signal is `deleteLater()`ed
  and our pointer to it nulled at once. Sizes are logical pixels (Qt scales them). The targets compile with
  `QT_NO_KEYWORDS`, `QT_NO_CAST_FROM_ASCII`/`TO_ASCII` and deprecations capped at 6.8; include Qt headers as
  `<QWidget>`. The only `#ifdef` platform split of the layer is `src/qt/platform.cpp`
