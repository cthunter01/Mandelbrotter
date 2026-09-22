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

Other presets: `clang-release`, `gcc-debug`, `gcc-release`, `tsan`, `coverage`, `ci-gcc`, `ci-clang`
(`gcc-debug` and the like are configure/build/test presets, not workflows).
Each builds into `build/<preset>/`; never edit anything under `build/`. The first configure of a preset
downloads and compiles wxWidgets (minutes); ccache makes later presets fast. A preset is only available on the
platforms it supports (`gcc-*`: Linux; `clang-*`: Linux and macOS; `msvc-*`: Windows); `cmake --list-presets`
shows this machine's.

## Layout
- `include/Mandelbrotter/`: public headers of the core library
- `src/core/`: `Mandelbrotter_lib` — kernels, viewport, palettes, progressive renderer, exporter, PNG writer,
  bookmarks (JSON), CLI. **No wxWidgets here**, ever: the tests and the `--render` path must work headless
- `src/gui/`: `Mandelbrotter_gui` — the wxWidgets layer; headers live beside sources, included as `"gui/x.h"`
- `src/main.cpp`: the executable. `wxIMPLEMENT_APP_NO_MAIN` must stay in this file (in a static library the
  linker drops the app initializer); `main()` parses the command line, runs the CLI, or hands the resolved
  settings to the GUI and calls `wxEntry` with no arguments
- `tests/`: GoogleTest files, named `*_test.cpp`, all in `Mandelbrotter_tests`; `cli_render_test.cmake` runs
  the real executable with `DISPLAY` cleared
- `cmake/ProjectOptions.cmake`: `Mandelbrotter_configure_target()` (warnings, sanitizers, coverage, tidy)
- `cmake/Dependencies.cmake`: third-party libraries via FetchContent (GoogleTest, nlohmann/json, stb, wxWidgets
  with its option block: static, native toolkit, unneeded components off, no `find_package` fallback because a shared
  system wx would otherwise be picked up)

## Conventions
- Headers are `.h` (never `.hpp`) and use `#pragma once`
- Code lives in `namespace mandelbrotter` (GUI: `mandelbrotter::gui`); project includes use quotes:
  `#include "Mandelbrotter/kernel.h"`
- Every new target must call `Mandelbrotter_configure_target(<target>)`
- New source files go into the relevant `CMakeLists.txt`; new tests go into `tests/CMakeLists.txt`
- Warnings are part of the build: code must compile cleanly with `-Werror` under GCC and Clang and with `/WX`
  under MSVC, and pass clang-tidy on Linux (`.clang-tidy`; `src/gui/.clang-tidy` and `tests/.clang-tidy` relax a few checks for wx and gtest)
- Code must build and pass its tests on Linux, macOS and Windows (CI runs all three). Use the standard library
  (`<filesystem>`, `<thread>`, `<chrono>`) over POSIX or Win32 APIs; when an OS API is unavoidable, keep it in one
  source file behind an `#ifdef _WIN32` / `__APPLE__` / `__linux__` split, with a branch for each platform
- Renderer callbacks run on worker threads: marshal to the GUI thread with `CallAfter`, never touch wx objects
  from a worker
- wxWidgets rules: `Bind()` only (no event tables, no `wxIMPLEMENT_DYNAMIC_CLASS`); convert text with
  `toWx()`/`fromWx()` from `gui/wx_util.h` (UTF-8), never `wxString(std::string)`; keep string literals given
  to wx ASCII-only (a non-ASCII narrow literal becomes an empty `wxString` under the C locale); no
  `wxString::Format`/`wxLogXXX` varargs (use `std::format` + `toWx`)
