# The help book

These files are the in-app help (Help > Contents), embedded into the executable at build time; nothing here is
installed separately. The wx build zips this directory (`cmake/HelpBook.cmake`), embeds the zip as string
literals and shows it with `wxHtmlHelpController`. The Qt build embeds the files as Qt resources (`src/CMakeLists.txt`)
and shows them in its own help window (`src/qt/HelpWindow.cpp`), which reads the same contents and index files.

## Files

- `help.hhp`: the book's options (title, start page, contents and index files).
- `contents.hhc`: the contents tree; `index.hhk`: the keyword index. Both use the HTML Help sitemap
  form: `<LI><OBJECT type="text/sitemap"><param name="Name" ...><param name="Local" ...></OBJECT>`,
  where `Local` is a page, optionally with `#anchor`.
- `*.html`: the pages. `images/`: the wx build's screenshots (`ui-*.png`) and rendered pictures (everything
  else). `images-qt/`: the Qt build's screenshots, the same `ui-*.png` names; the Qt build embeds them in place of
  the wx ones, so the pages refer to `images/ui-*.png` either way.

## Writing pages

wxHTML renders a subset of HTML 3.2 (so does the Qt help window's `QTextBrowser`, and more): `h1`-`h3`, `p`, `b`, `i`, `tt`, `pre`, `ul`/`ol`, `table` (with
`border`/`cellpadding`), `img`, `a href`/`a name`, `hr`. No CSS, no `style` or `script`. Keep the
files ASCII (write `&gt;` for `>`), give every page a `<title>` (the window title and the search
results use it) and end it with the `Contents` footer link the other pages have.

Links that act on the main window instead of opening a page use the `mandelbrotter:` scheme and
start with "Try it:":

| Link | Effect |
|---|---|
| `mandelbrotter:view --center -0.7436,0.1318 --zoom 5000 --palette electric` | the current view with these command-line options applied (a family or Julia change without `--center`/`--zoom` starts from that fractal's default view) |
| `mandelbrotter:flight seahorse-dive` | plays a built-in flight (`builtinFlights()` in `flights.h`) |
| `mandelbrotter:tour` | starts the guided tour |
| `mandelbrotter:orbit on` / `off` | the orbit overlay |
| `mandelbrotter:reset` | the current fractal's default view |
| `mandelbrotter:export` | opens the Save image as PNG dialog |

`tests/help_book_tests.cpp` checks every page: the control files, every link and image, every
`mandelbrotter:` action, that `images-qt/` holds exactly the wx set's screenshot names, and the image budget: 5 MiB
per build, the rendered pictures plus one toolkit's screenshots. Run `ctest --preset clang-debug -R HelpBook`.

## Regenerating the images

The images are committed. After a change to the window or to the rendered examples
(`helpImageSpecs()` in `src/core/help_images.cpp`), regenerate them all with a release build on a
Linux desktop (the wx screenshots show the GTK look on every platform):

    cmake --build --preset clang-release
    ./build/clang-release/bin/Mandelbrotter --screenshots docs/help/images

The run renders the example pictures headlessly, then opens the window, walks it through the
documented states, captures each (`src/gui/ScreenshotRun.cpp`, `src/gui/window_capture.cpp`) and
quits. Keep the mouse off the window while it runs. On Wayland, if it reports that the capture
returned nothing, run it again with `GDK_BACKEND=x11`. It never touches your own bookmarks: the
bookmarks it shows come from a scratch file.

The Qt build's screenshots come from its own run, which writes only the `ui-*.png` files (the rendered
pictures are the same in every build). It runs on Qt's offscreen platform with the Fusion style, so it
needs no desktop and looks the same on every machine that has the same fonts:

    cmake --build --preset clang-release-qt
    QT_QPA_PLATFORM=offscreen:configfile=tests/qt/offscreen.json \
        ./build/clang-release-qt/bin/Mandelbrotter --screenshots docs/help/images-qt

Run it twice, rebuilding in between, so the help window's screenshot shows the new pictures (the Qt build
embeds them). The script both builds follow is `src/app/ScreenshotScript.cpp`.
