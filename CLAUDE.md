# Mandelbrotter

Interactive Mandelbrot-family fractal explorer. C++23 project built with CMake presets + Ninja, wxWidgets 3.2 GUI
(statically linked, built in-tree via FetchContent), tested with GoogleTest. Cross-platform: Linux (GCC, Clang),
macOS (Apple Clang) and Windows (MSVC).

## Commands
- Build and test (Clang Debug): `cmake --workflow --preset dev`
- Rebuild only: `cmake --build --preset clang-debug`
- Test only: `ctest --preset clang-debug`
- One test: `ctest --preset clang-debug -R 'Kernel\.'` or `build/clang-debug/bin/Mandelbrotter_tests --gtest_filter='Kernel.*'`
- Run the app: `build/clang-debug/bin/Mandelbrotter` (use `clang-release` for interactive speed);
  headless render: `build/clang-debug/bin/Mandelbrotter --render out.png --size 640x400`
- Before finishing a change, also run: `cmake --workflow --preset tidy` (clang-tidy, warnings are errors) and
  `cmake --workflow --preset asan` (AddressSanitizer + UBSan); `cmake --workflow --preset tsan` for renderer changes
- On Windows the presets are `msvc-debug` (workflow `dev-msvc`), `msvc-release` and `ci-msvc`, and cmake must run
  in a Developer PowerShell for VS. `tidy`, `asan`, `tsan` and `coverage` exist on Linux and macOS only
- Formatting is automatic: a Claude Code hook (`.claude/hooks/format-cpp.sh`) runs clang-format on every C/C++
  file right after you edit it. The pre-commit hook and CI also reject unformatted files
- Help book images: after a change to the window or to `helpImageSpecs()`, regenerate them with a release build on
  a Linux desktop: `GDK_BACKEND=x11 build/clang-release/bin/Mandelbrotter --screenshots docs/help/images` (twice
  when the rendered pictures changed, so the help-window screenshot shows them); `ctest --preset clang-debug -R
  HelpBook` checks the book

Other presets: `clang-release`, `gcc-debug`, `gcc-release`, `tsan`, `coverage`, `ci-gcc`, `ci-clang`
(`gcc-debug` and the like are configure/build/test presets, not workflows).
Each builds into `build/<preset>/`; never edit anything under `build/`. The first configure of a preset
downloads and compiles wxWidgets (minutes); ccache makes later presets fast. A preset is only available on the
platforms it supports (`gcc-*`: Linux; `clang-*`: Linux and macOS; `msvc-*`: Windows); `cmake --list-presets`
shows this machine's.

## Layout
- `include/Mandelbrotter/`: public headers of the core library
- `src/core/`: `Mandelbrotter_lib` — kernels, viewport, palettes, progressive renderer, exporter, PNG writer,
  bookmarks (JSON), CLI, and the deep-zoom machinery: `BigFixed`/`BigComplex` (fixed-point big numbers),
  `ReferenceOrbit` and `perturbation.h`. **No wxWidgets here**, ever: the tests and the `--render` path must
  work headless
- `src/gui/`: `Mandelbrotter_gui` — the wxWidgets layer; headers live beside sources, included as `"gui/x.h"`.
  Besides the frame, canvas, panel and dialogs: `HelpController` (the wxHTML help window over the embedded book),
  `DemoPlayer` (plays a core `Flight`), `GuidedTour` + `TourCard` (the tour; the card is a child panel of the
  frame, never a top-level window, because Wayland does not let apps position those), `ScreenshotRun` (the
  `--screenshots DIR` developer mode) and `window_capture.cpp`, the one file with per-platform code
- `src/tools/`: build-time tools; `embed_file.cpp` (`Mandelbrotter_embed`) turns the zipped help book into a C++
  source of string-literal chunks (`cmake/HelpBook.cmake`)
- `docs/help/`: the help book: `help.hhp`, `contents.hhc`, `index.hhk`, hand-written pages in wxHTML (ASCII, no
  CSS) and `images/` (committed; `ui-*.png` are screenshots, the rest rendered examples from `helpImageSpecs()`).
  Zipped and embedded at build time; `tests/help_book_tests.cpp` checks every link, image and action link.
  Authoring notes in `docs/help/README.md`
- `src/main.cpp`: the executable. `wxIMPLEMENT_APP_NO_MAIN` must stay in this file (in a static library the
  linker drops the app initializer); `main()` parses the command line, runs the CLI, or hands the resolved
  settings to the GUI and calls `wxEntry` with no arguments
- `tests/`: GoogleTest files, all in `Mandelbrotter_tests` (class tests: `MyClassTests.cpp`; other tests:
  `*_tests.cpp`); `cli_render_test.cmake` runs the real executable with `DISPLAY` cleared
- `cmake/ProjectOptions.cmake`: `Mandelbrotter_configure_target()` (warnings, sanitizers, coverage, tidy)
- `cmake/Dependencies.cmake`: third-party libraries via FetchContent (GoogleTest, nlohmann/json, stb, wxWidgets
  with its option block: static, native toolkit, unneeded components off except HTML and the help subsystem for the
  in-app help, no `find_package` fallback because a shared system wx would otherwise be picked up)

## Conventions
- Headers are `.h` (never `.hpp`) and use `#pragma once`
- A class's header and implementation files are named exactly after the class, including capitalization:
  `class MyClass` lives in `include/Mandelbrotter/MyClass.h` and `src/core/MyClass.cpp` (GUI classes: `src/gui/MyClass.h`
  and `.cpp`), and its tests in `tests/MyClassTests.cpp`. Headers of free functions or of several small types keep
  snake_case names (`kernel.h`, `geometry.h`), with tests named `*_tests.cpp`
- Code lives in `namespace mandelbrotter` (GUI: `mandelbrotter::gui`); project includes use quotes:
  `#include "Mandelbrotter/kernel.h"`
- Every new target must call `Mandelbrotter_configure_target(<target>)`
- New source files go into the relevant `CMakeLists.txt`; new tests go into `tests/CMakeLists.txt`
- Warnings are part of the build: code must compile cleanly with `-Werror` under GCC and Clang and with `/WX`
  under MSVC, and pass clang-tidy on Linux (`.clang-tidy`; `src/gui/.clang-tidy` and `tests/.clang-tidy` relax a few checks for wx and gtest)
- Code must build and pass its tests on Linux, macOS and Windows (CI runs all three). Use the standard library
  (`<filesystem>`, `<thread>`, `<chrono>`) over POSIX or Win32 APIs; when an OS API is unavoidable, keep it in one
  source file behind an `#ifdef _WIN32` / `__APPLE__` / `__linux__` split, with a branch for each platform
- Deep zoom: `ViewSpec::center` is a `BigComplex` whose precision follows the zoom (`fractionBitsFor`);
  kernels never see absolute big positions, only double offsets from the centre (`Viewport::offsetFromCenter`).
  Above `kPerturbationZoom` (1e8) the renderer iterates every pixel as a delta from the centre's
  `ReferenceOrbit` (`iteratePerturbed`, with rebasing) and jumps stretches where the delta is tiny through the
  orbit's `BlaTable` (bilinear approximation; `kBlaEpsilon` trades speed for the last digits); below it the
  direct double kernel runs. Bookmarks and
  view files store centres as decimal strings (schema version 2); `BigFixed::fromDecimal` rounds to nearest
  and `toDecimal` prints enough digits that a saved view reloads bit-for-bit
- Renderer callbacks run on worker threads: marshal to the GUI thread with `CallAfter`, never touch wx objects
  from a worker
- Help actions: links of the form `mandelbrotter:<verb> ...` in the book (`help_action.h`: `view <CLI options>`,
  `flight <id>`, `tour`, `orbit on|off`, `reset`, `export`) are executed by `MainFrame::runHelpAction`; `view`
  reuses the CLI parser, so the words are the same as on the command line. Demo flights are data in
  `flights.h` (`builtinFlights()`), interpolated by pure functions; the Demos menu, the demos page and the
  `place-*` images all derive from them. The Save image dialog is modeless (the tour and the screenshot mode
  drive it); `MainFrame`'s public methods are the automation surface the help system uses
- wxWidgets rules: `Bind()` only (no event tables, no `wxIMPLEMENT_DYNAMIC_CLASS`); convert text with
  `toWx()`/`fromWx()` from `gui/wx_util.h` (UTF-8), never `wxString(std::string)`; keep string literals given
  to wx ASCII-only (a non-ASCII narrow literal becomes an empty `wxString` under the C locale); no
  `wxString::Format`/`wxLogXXX` varargs (use `std::format` + `toWx`)
