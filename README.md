# Mandelbrotter

An interactive explorer for the Mandelbrot set and its relatives, written in C++23 with a wxWidgets GUI.
It builds and runs on Linux, macOS and Windows.

- **Fractals**: Mandelbrot and Multibrot (`z^n + c`, n = 2..8), Burning Ship, Tricorn, and the Julia set of any
  of them (pick the constant by clicking on the set, type it in, or watch the live preview follow the mouse).
- **Navigation**: mouse wheel zooms at the cursor, drag pans, right-drag or Shift-drag zooms to a rectangle,
  right-click zooms out, arrow keys / `+` / `-` / `Home` work too.
- **Rendering**: multi-threaded and progressive (a coarse pass appears instantly, then refines); any navigation
  cancels the render in flight. Iteration limit (up to 1 000 000) is manual or grows automatically with the zoom.
- **Deep zoom**: to 1e300, far past where doubles stop telling pixels apart (about 1e13). The view centre is a
  big number whose precision follows the zoom, and above 1e8 every pixel is iterated as a small delta from the
  centre's reference orbit (perturbation with rebasing), for every fractal family and in Julia mode. Bilinear
  approximation jumps the long stretches where a pixel's delta is tiny against the orbit, so deep frames take a
  fraction of the time of iterating them step by step.
- **Colouring**: smooth escape-time colouring with six palettes plus density and offset controls; palette
  changes recolour instantly without recomputing.
- **Extras**: orbit overlay for the point under the cursor; supersampled PNG export at any resolution; copy to
  clipboard; bookmarks and view files (JSON); a headless `--render` mode for scripting.

## Requirements

On every platform: CMake 3.28+, Ninja and Git. wxWidgets is downloaded and built from source as static
libraries by the first configure (a few minutes); GoogleTest and nlohmann/json are used from the system if
installed, otherwise downloaded.

- **Linux**: GCC 14+ or Clang 18+ (C++23 including `<print>` and `<expected>`), plus the GTK 3 development
  files and `pkg-config` (GTK itself stays a system library). On Arch: `pacman -S cmake ninja gtk3 pkgconf`;
  on Debian/Ubuntu: `apt install cmake ninja-build libgtk-3-dev pkg-config`.
- **macOS**: Xcode 26.6 or later (its Apple Clang is what CI uses; earlier Xcode 26 releases are untested), and
  Ninja and CMake from Homebrew: `brew install cmake ninja`. wxWidgets uses Cocoa, so nothing else is needed.
- **Windows**: Visual Studio 2026 (what CI uses; Visual Studio 2022 17.8 or later should also work) with the
  *Desktop development with C++* workload, which includes MSVC, CMake and Ninja. Run every `cmake` command
  from a *Developer PowerShell for VS* (x64) so the compiler is on the PATH. Symlinks need Developer Mode
  turned on; without it the build still works, only the `compile_commands.json` link for clangd is skipped.
- Optional, all platforms: clang-format, ccache (strongly recommended: every preset builds wxWidgets once, and
  ccache shares the result between presets with the same flags), Doxygen. Linux and macOS only: clang-tidy and
  llvm-cov/llvm-profdata for the `tidy` and `coverage` presets (on macOS from `brew install llvm`; the presets
  find them there without touching your PATH).

## Quick start

Linux and macOS:

```sh
cmake --workflow --preset dev          # configure + build + test (Clang, Debug)
./build/clang-debug/bin/Mandelbrotter  # open the window
```

Windows, in a Developer PowerShell for VS:

```powershell
cmake --workflow --preset dev-msvc         # configure + build + test (MSVC, Debug)
.\build\msvc-debug\bin\Mandelbrotter.exe   # open the window
```

Debug builds render slowly; for exploring, build the release preset:

```sh
cmake --workflow --preset clang-release    # Windows: msvc-release
./build/clang-release/bin/Mandelbrotter
```

## Command line

```
usage: Mandelbrotter [options]

  -h, --help              show this help and exit
  -o, --render FILE.png   render headlessly to a PNG file and exit
      --view FILE.json    start from a saved view (File > Export view..., or a bookmark)
      --fractal NAME      mandelbrot (default), burning-ship or tricorn
      --exponent N        z^N + c, N from 2 to 8 (default 2)
      --julia RE,IM       draw the Julia set for the constant c = RE + IM i
      --center RE,IM      centre of the view
      --zoom Z            magnification (1 shows the whole set; up to 1e+300)
      --iterations N      fixed iteration limit (default: automatic, grows with zoom)
      --palette NAME      one of: classic, grayscale, fire, ocean, rainbow, electric
      --size WxH          output size for --render (default 1920x1080)
      --supersample K     anti-aliasing for --render: 1, 2 or 4 samples per axis (default 1)
```

Examples:

```sh
# Seahorse valley, anti-aliased, no window needed
Mandelbrotter --render seahorse.png --center -0.7436,0.1318 --zoom 5000 --iterations 2000 \
              --palette electric --supersample 2

# The Burning Ship's little ship
Mandelbrotter --render ship.png --fractal burning-ship --center -1.75,-0.03 --zoom 40 --palette fire

# Open the window on a saved view
Mandelbrotter --view my-view.json
```

Views and bookmarks are JSON; the centre is written as decimal strings with as many digits as the zoom needs,
and `--center` takes decimals of any length. Bookmarks live in the per-user application data directory:
`~/.local/share/Mandelbrotter/bookmarks.json` on Linux, `~/Library/Application Support/Mandelbrotter/bookmarks.json`
on macOS and `%APPDATA%\Mandelbrotter\bookmarks.json` on Windows.

## Presets

Every preset builds into `build/<preset>/`. A preset only exists on the platforms whose compiler it uses, so
`cmake --list-presets` shows the ones available on this machine.

| Preset | Platforms | What it does |
|---|---|---|
| `dev` (workflow) | Linux, macOS | Clang, Debug: configure, build, test |
| `dev-msvc` (workflow) | Windows | MSVC, Debug: configure, build, test |
| `clang-debug`, `clang-release` | Linux, macOS | configure/build/test presets per build type |
| `gcc-debug`, `gcc-release` | Linux | the same with GCC |
| `msvc-debug`, `msvc-release` | Windows | the same with MSVC |
| `asan` | Linux, macOS | Clang Debug with AddressSanitizer + UndefinedBehaviorSanitizer |
| `tsan` | Linux, macOS | Clang RelWithDebInfo with ThreadSanitizer |
| `tidy` | Linux, macOS | Clang Debug with clang-tidy, warnings as errors |
| `coverage` | Linux, macOS | Clang Debug with source-based coverage; report in `build/coverage/coverage/` |
| `ci-gcc`, `ci-clang`, `ci-msvc` | Linux / Linux, macOS / Windows | Release, warnings as errors (what CI runs) |

Configure, build and test can also be run separately: `cmake --preset clang-debug`,
`cmake --build --preset clang-debug`, `ctest --preset clang-debug`.

CI (GitHub Actions) runs `ci-gcc`, `ci-clang`, `asan` and `tidy` on Arch Linux, `ci-clang` on macOS, `ci-msvc` on
Windows, and a clang-format check.

## Layout

- `include/Mandelbrotter/`, `src/core/`: the core library (`Mandelbrotter_lib`). Fractal kernels, viewport
  maths, palettes, the progressive multi-threaded renderer, PNG export, bookmarks and the command-line parser.
  No GUI dependency, fully unit-tested.
- `src/gui/`: the wxWidgets layer (`Mandelbrotter_gui`): main frame, canvas, side panel, dialogs.
- `src/main.cpp`: the executable; dispatches between the CLI and the GUI.
- `tests/`: GoogleTest suites for the core, plus a headless integration test that runs the real executable.
- `cmake/`: dependency and build configuration.

Doxygen documentation: `cmake --build --preset clang-debug --target docs` (needs Doxygen).
