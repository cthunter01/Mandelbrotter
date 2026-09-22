# Mandelbrotter

An interactive explorer for the Mandelbrot set and its relatives, written in C++23 with a wxWidgets GUI.

- **Fractals**: Mandelbrot and Multibrot (`z^n + c`, n = 2..8), Burning Ship, Tricorn, and the Julia set of any
  of them (pick the constant by clicking on the set, type it in, or watch the live preview follow the mouse).
- **Navigation**: mouse wheel zooms at the cursor, drag pans, right-drag or Shift-drag zooms to a rectangle,
  right-click zooms out, arrow keys / `+` / `-` / `Home` work too.
- **Rendering**: multi-threaded and progressive (a coarse pass appears instantly, then refines); any navigation
  cancels the render in flight. Iteration limit is manual or grows automatically with the zoom.
- **Colouring**: smooth escape-time colouring with six palettes plus density and offset controls; palette
  changes recolour instantly without recomputing.
- **Extras**: orbit overlay for the point under the cursor; supersampled PNG export at any resolution; copy to
  clipboard; bookmarks and view files (JSON); a headless `--render` mode for scripting.

## Requirements

- CMake 3.28+ and Ninja
- GCC 14+ or Clang 18+ (C++23 including `<print>` and `<expected>`)
- GTK 3 development files and `pkg-config` (wxWidgets is downloaded and built from source as static libraries;
  GTK itself stays a system library). On Arch: `pacman -S gtk3 pkgconf`; on Debian/Ubuntu:
  `apt install libgtk-3-dev pkg-config`.
- Optional: clang-tidy, clang-format, llvm-cov/llvm-profdata (coverage), Doxygen, ccache (strongly recommended:
  every preset builds wxWidgets once, and ccache shares the result between presets with the same flags)

GoogleTest and nlohmann/json are used from the system if installed, otherwise downloaded.

## Quick start

```sh
cmake --workflow --preset dev          # configure + build + test (Clang, Debug)
./build/clang-debug/bin/Mandelbrotter  # open the window
```

The first configure downloads and compiles wxWidgets (a few minutes). Debug builds render slowly; for
exploring, build the release preset:

```sh
cmake --workflow --preset clang-release
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
      --zoom Z            magnification (1 shows the whole set; up to 1e+12)
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

Views and bookmarks are JSON. Bookmarks live in `~/.local/share/Mandelbrotter/bookmarks.json`.

## Presets

| Preset | What it does |
|---|---|
| `dev` (workflow) | Clang, Debug: configure, build, test |
| `clang-debug`, `clang-release`, `gcc-debug`, `gcc-release` | configure/build/test presets per compiler and build type |
| `asan` | Clang Debug with AddressSanitizer + UndefinedBehaviorSanitizer |
| `tsan` | Clang RelWithDebInfo with ThreadSanitizer |
| `tidy` | Clang Debug with clang-tidy, warnings as errors |
| `coverage` | Clang Debug with source-based coverage; report in `build/coverage/coverage/` |
| `ci-gcc`, `ci-clang` | Release, warnings as errors (what CI runs) |

Configure, build and test can also be run separately: `cmake --preset clang-debug`,
`cmake --build --preset clang-debug`, `ctest --preset clang-debug`.

## Layout

- `include/Mandelbrotter/`, `src/core/`: the core library (`Mandelbrotter_lib`). Fractal kernels, viewport
  maths, palettes, the progressive multi-threaded renderer, PNG export, bookmarks and the command-line parser.
  No GUI dependency, fully unit-tested.
- `src/gui/`: the wxWidgets layer (`Mandelbrotter_gui`): main frame, canvas, side panel, dialogs.
- `src/main.cpp`: the executable; dispatches between the CLI and the GUI.
- `tests/`: GoogleTest suites for the core, plus a headless integration test that runs the real executable.
- `cmake/`: dependency and build configuration.

Doxygen documentation: `cmake --build --preset clang-debug --target docs` (needs Doxygen).
