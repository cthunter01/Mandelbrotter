# Toolkit abstraction: a wx-free application layer

Design and implementation plan for separating Mandelbrotter's application logic from the wxWidgets toolkit, so
that the GUI becomes a thin view and a second front end (Qt) could be written against the same layer.
Written 2026-09-26 from a survey of the code at commit `d941c5b` (version 0.8.3).

## 1. Summary

- The rendering engine (`Mandelbrotter_lib`, ~5100 lines) is already independent of wxWidgets and the build
  enforces it. Nothing there changes.
- The application behaviour is not: the model, navigation state machine, render bookkeeping, demo playback,
  guided tour, and the bookmark and snapshot policies all live inside wx subclasses in `src/gui/` (~3800 lines
  of `.cpp`), with no tests, because the test binary links only the core.
- This plan introduces a new static library **`Mandelbrotter_app`** (`src/app/`, `include/Mandelbrotter/app/`,
  `namespace mandelbrotter::app`, links only the core, never wx) that holds that logic behind `std::function`
  hook structs, with GoogleTest coverage, and rewires the wx classes to be views over it.
- **Scope decisions:** abstraction only, no Qt code in this branch (the unit tests are the proof a second
  toolkit could sit on the layer); a new library rather than folding the logic into the core; work on branch
  `app-layer`, one commit per green step; nothing pushed until asked; no version bump; the `Release` workflow is
  never run from this branch.
- Behaviour stays identical, including every documented constant and text, except for two guided-tour bugs
  that the extraction makes testable (section 7).

## 2. Current state

### 2.1 What is cleanly separated

- `Mandelbrotter_lib` links only Threads, nlohmann/json and stb (`src/CMakeLists.txt`). No file in `src/core/`
  or `include/` mentions wx. The tests and the `--render` / `--flight-frames` paths run headless, so wx cannot
  leak into the core without something breaking.
- The core's interfaces are toolkit-neutral:
  - `ProgressiveRenderer::start(RenderJob, TileCallback, CompletionCallback)` reports tiles and completion
    through `std::function` callbacks on worker threads, tagged with generation ids; `cancel()` is synchronous.
  - `RgbImage` is packed 8-bit RGB (maps onto `wxImage` today and `QImage::Format_RGB888` tomorrow).
  - `Viewport` does the navigation maths (`zoomedAt`, `panned`, `zoomedToRect`, `resized`, `toPixel`,
    `pixelCenter`, `pixelCenterBig`); `RenderSettings` + `needsRerender` + `effectiveIterations` is a single
    value-type model.
  - Flight interpolation (`flights.h`), help-action parsing and `resolveViewAction` (`help_action.h`),
    bookmark and view JSON (`bookmarks.h`), the exporter with `stop_token` and progress callback
    (`exporter.h`), and the CLI (`cli.h`) are all core.
- Most GUI classes already talk to each other through callbacks, not wx events (`FractalCanvas::onViewChanged`,
  `SidePanel::onSettingsChanged`, `DemoPlayer::Hooks`).

### 2.2 Where it is tied to wx

There is no toolkit-neutral application layer between the core and the widgets. Logic that is really
application code lives in wx subclasses:

| File (lines) | Application logic hiding there | Genuinely wx |
|---|---|---|
| `MainFrame` (785) | Owns the `RenderSettings` model, `BookmarkStore`, `DemoPlayer`, snapshot ("Back to where I was"), the tour's temporary bookmark; policies: family/Julia change resets the view, seed pick switches to Julia with a default view, demos and the tour stop each other, `addBookmark` stops demos so the temporary bookmark is never saved, `deleteBookmark` during the tour only stops the tour, help-action dispatch, status-bar texts, context-help routing, Esc/any-key handling | Menus, file/text/message/about dialogs, clipboard, sizer show/hide, status bar widget |
| `FractalCanvas` (615) | Render bookkeeping (stale-generation tiles dropped, first-pass tile counting for `hasCoarsePicture`, per-tile colouring, shifted old picture kept after a pan, recolour vs re-render), the input state machine (3 px drag threshold, 1.25x per wheel notch, 2x key zoom, 10 % key pan, rubber band, right-click zoom out), orbit computation (exact `ReferenceOrbit` above the perturbation zoom, `orbit()` below, deep-Julia z0 rule), overlay geometry | Painting, `wxBitmap`, `wxTimer` resize debounce, mouse capture, cursors, key codes |
| `DemoPlayer` (87) | Whole class is neutral: wall-clock sampling of a flight, frame dropping while the canvas is busy, status text | `wxEvtHandler` base, `wxTimer` |
| `GuidedTour` (366) | The 11 steps (title, text, help page, target, action), the state machine (start/stop/next/back/showStep), baseline restore | Card creation, anchor rectangles, section highlight/scroll, frame-resize re-placement |
| `TourCard` (135) | `placeNear` geometry, "Step N of M", Back/Finish/Learn-more enable rules | Everything else |
| `SidePanel` (512) | Settings copy + re-entrancy guard, control-to-field mapping with clamps, seed parse/restore, offset<->slider conversion, preview-seed policy, index lookups | Widget tree, scrolling, highlight ring |
| `JuliaPreview` (128) | Preview settings (128 iterations, auto off, default view), 50 ms throttle | Synchronous `renderSync` on the GUI thread, painting |
| `ExportDialog` (78) | `{1, 2, 4}` supersample table, clamping | Widgets |
| `export_runner` (91) | Worker thread + atomic progress + promise/future + cancel | `wxProgressDialog` poll loop, message box |
| `HelpController` (144) | Link classification (action / external / page), `helpBookZip()` | `wxHtmlHelpController`, memory filesystem, help frame |
| `MandelbrotterApp` (122) | `StartupOptions`, scratch bookmark seeding for `--screenshots`, exit-code rule | `wxApp`, `wxStandardPaths`, image/FS handlers |
| `ScreenshotRun` (444) | The 15-shot table, prepare -> wait for render -> settle -> capture -> cleanup state machine, crop/scale maths | Window rectangles, `GetStatusBar`, dialogs, `window_capture` |
| `wx_util` (63) | `formatCenter`, `formatZoom` (pure, untested, hidden behind wx includes), unused `formatComplex` | `toWx`, `fromWx`, `toWxImage` |
| `BookmarkStore` (50) | Already pure C++; only lives in `gui/` by accident | - |

Other findings from the survey:

- The scene constants and helpers (`kSeahorseValley`, `kJuliaSeed`, `kOrbitPoint`, `at`, `mandelbrotDefault`,
  `seahorse`) are written three times: `GuidedTour.cpp:26-50`, `ScreenshotRun.cpp:47-80` (with deep zoom 1e12
  versus the tour's 1e10) and `MandelbrotterApp.cpp:40-66` (`viewAt`).
- The help book is the biggest non-code tie: wxHTML help format (`.hhp`/`.hhc`/`.hhk`, no CSS) shown by
  `wxHtmlHelpController`. That stays toolkit-specific; only link classification and page routing move.
- The build fetches wxWidgets unconditionally (`cmake/Dependencies.cmake`). A `MANDELBROTTER_BUILD_GUI` option is
  a natural follow-up once the layer exists, but is out of scope here.
- Four latent issues in the tour's state handling, found by reading `GuidedTour.cpp`:
  1. The orbit overlay is only set in step 6, so going Back from step 7 or later to steps 0-5 leaves it on.
  2. The temporary bookmark from step 7 stays through steps 8-9. **Not a bug:** the step-7 card says the entry
     "will disappear when the tour ends".
  3. Step 10's `clearHighlights()` is undone straight away by `showStep`'s `highlight(CANVAS)`.
  4. Finish restores the baseline twice (step 10 and `stop()`). **Not a bug:** idempotent, and the second restore
     covers a user who zooms during the last step ("Your view, overlay and bookmarks are back as they were").

### 2.3 Why Qt rather than GTK

On Linux wxWidgets already runs on GTK 3; a native gtkmm port would gain little there and lose the native
Windows and macOS builds. Qt is the meaningful alternative, and the layer described here is what a Qt front end
would need.

## 3. Design

### 3.1 Principles

- **Hook structs, not interfaces.** The codebase already uses `std::function` members and `Hooks` structs
  (`DemoPlayer::Hooks`, the canvas and panel callbacks). The new layer follows that style rather than abstract
  base classes; a toolkit fills a struct of lambdas.
- **Logical coordinates at the boundary.** `FractalCanvas` works in logical window pixels and converts to device
  pixels with the content scale (`toDevice` = `floor(p * scale)`, `toLogical` = `lround(p / scale)`). The
  controller keeps exactly that, so the 3 px threshold and every rounding stay as they are.
- **The controller never touches a toolkit thread.** Worker-thread callbacks are handed to a `post` hook that the
  toolkit implements (wx: `CallAfter`; Qt: a queued `invokeMethod`; tests: a queue the test drains).
- **Views own their controllers where wx destruction order demands it.** wx destroys child windows in
  `~wxWindowBase`, after the frame's own members. So `FractalCanvas` owns its `CanvasController` as a member and
  `AppController` (a `MainFrame` member) holds a reference, the same way `GuidedTour` holds `MainFrame&` today.
- **Constants live in the layer.** Every number the help book documents (3 px, 1.25x, 2x, 10 %, 150 ms, 33 ms,
  50 ms, 128, 512, +-10000, 1000 slider steps) becomes an `inline constexpr` in an app header so a second toolkit
  cannot drift from it.

### 3.2 CMake

New target in `src/CMakeLists.txt`, between the core and the GUI:

```cmake
# Application layer: the window's logic without a toolkit (controllers, demo player, tour script, view-models).
add_library(Mandelbrotter_app STATIC
    app/AppController.cpp
    app/BookmarkStore.cpp
    app/CanvasController.cpp
    app/DemoPlayer.cpp
    app/ExportTask.cpp
    app/TourScript.cpp
    app/format.cpp
    app/help_routing.cpp
    app/panel_model.cpp
    app/scenes.cpp
    app/startup.cpp
    app/tour_layout.cpp)
add_library(Mandelbrotter::app ALIAS Mandelbrotter_app)
target_include_directories(Mandelbrotter_app PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(Mandelbrotter_app PUBLIC Mandelbrotter::lib)   # never wx
Mandelbrotter_configure_target(Mandelbrotter_app)
```

- `Mandelbrotter_gui` links `Mandelbrotter::app` (plus wx as today); `gui/BookmarkStore.cpp` and
  `gui/DemoPlayer.cpp` leave its source list; `GuidedTour.cpp` becomes `TourView.cpp`.
- `Mandelbrotter_tests` links `Mandelbrotter::app` and lists the new test files.
- The root `.clang-tidy` applies to `src/app` **unrelaxed** (include-cleaner and owning-memory stay on; the layer
  is plain C++). `cmake/Coverage.cmake` ignores only `(build|_deps|tests|gui)`, so the layer is measured.
  `cmake/Docs.cmake` already documents everything under `include/`.
- Naming follows CLAUDE.md: a class lives in `include/Mandelbrotter/app/MyClass.h` + `src/app/MyClass.cpp` with
  tests in `tests/MyClassTests.cpp`; free-function headers are snake_case with `tests/x_tests.cpp`.

### 3.3 Components

| New header (`include/Mandelbrotter/app/`) | Contents | Extracted from |
|---|---|---|
| `format.h` | `formatCenter`, `formatZoom` (verbatim), `formatSeedComponent` (`"{:.10g}"`), `defaultBookmarkName(settings)` = `displayName(family) + " at " + formatZoom(zoom)`; unused `formatComplex` deleted | `wx_util.cpp:35-61`, `SidePanel.cpp:48-51`, `MainFrame.cpp:727-729` |
| `scenes.h` | `kSeahorseValley`, `kJuliaSeed`, `kOrbitPoint`, `kTourDeepZoom = 1e10`, `kScreenshotDeepZoom = 1e12`; `viewAt(base, center, zoom)` (`clampZoom` + `fractionBitsFor`), `mandelbrotDefault()`, `burningShipDefault()`, `juliaExample()`, `seahorse(palette = "electric")`, `seahorseFire()` (density 32, offset 0.25), `deepSeahorse(zoom)` (last keyframe of `seahorse-dive`, re-centred at `zoom`), `screenshotBookmarks()` (the three scratch bookmarks) | the triplicated code in `GuidedTour.cpp`, `ScreenshotRun.cpp`, `MandelbrotterApp.cpp` |
| `startup.h` | `StartupOptions{settings, screenshotsDir}`, `kScratchDirName = "Mandelbrotter-screenshots"`, `scratchBookmarksPath(tempDir)` (creates and seeds), `bookmarksPathFor(options, userDataDir, tempDir)` | `MandelbrotterApp.cpp:28-68`; wx keeps the `wxStandardPaths` lookup and the static setter/getter |
| `help_routing.h` | `HelpContext{Area{NONE, CANVAS, PANEL, EXPORT_DIALOG}, optional<PanelSection>, juliaControl}`, `helpPageFor(HelpContext)`; `HelpLinkKind{ACTION, EXTERNAL, PAGE}`, `classifyHelpLink(url)` | `MainFrame.cpp:82-98, 453-470`; `HelpController.cpp:119-141` |
| `panel_model.h` | `PanelSection` enum (SidePanel keeps `using Section = app::PanelSection;`), `kOffsetSliderSteps = 1000`, `kPreviewIterations = 128`, `kPreviewThrottle = 50ms`; pure functions `offsetToSlider` / `sliderToOffset`, `parseSeed(re, im)` (`parseNumber<double>(trimSpaces())`, nullopt if either fails), `previewSeedFor(spec, hovered)` (Julia mode -> the fixed seed), `familyIndex` / `paletteIndex`, `effectiveLimitText`, `previewSettings(spec, seed, coloring)` | `SidePanel.cpp:306-433`, `JuliaPreview.cpp:99-105` |
| `tour_layout.h` | `placeTourCard(anchor, card, parent, gap) -> PixelPoint` | `TourCard.cpp:98-123` |
| `BookmarkStore.h` | moved unchanged (namespace only) | `gui/BookmarkStore.*` |
| `DemoPlayer.h` | see 3.4 | `gui/DemoPlayer.*` |
| `CanvasController.h` | see 3.5 | `FractalCanvas.cpp` |
| `AppController.h` | see 3.6 | `MainFrame.cpp` |
| `TourScript.h` | see 3.7 | `GuidedTour.cpp` |
| `ExportTask.h` | see 3.8 | `export_runner.cpp:33-90` |

Why pure functions rather than a `PanelModel` class for the side panel: its model is already the value type
`RenderSettings`, and its only logic is conversions. A class would need a dozen per-control push hooks and would
remove no wx code.

`wx_util.h` keeps `toWx`/`fromWx`/`toWxImage` and gains the small coordinate converters the thin views need:
`toWx(PixelPoint)`, `fromWx(wxPoint)`, `toWx(PixelRect)`, `fromWx(wxSize) -> PixelSize`. `PixelPoint`
arithmetic helpers stay file-local in `src/app/*.cpp`; the core's `geometry.h` is not extended in this branch.

### 3.4 `DemoPlayer`

```cpp
inline constexpr auto kDemoTick = std::chrono::milliseconds(33);

class DemoPlayer
{
public:
    struct Hooks
    {
        std::function<void(const RenderSettings&)>             applyFrame;
        std::function<bool()>                                  canvasReady;
        std::function<void(std::string_view)>                  showStatus;
        std::function<void()>                                  finished;
        std::function<void(bool)>                              setTimerRunning;  // toolkit runs a kDemoTick timer
        std::function<std::chrono::steady_clock::time_point()> now;  // optional; tests inject a clock
    };
    explicit DemoPlayer(Hooks hooks);
    void play(const Flight& flight);
    void stop();
    void tick();  // called by the toolkit's timer
    [[nodiscard]] bool          playing() const noexcept;
    [[nodiscard]] const Flight* current() const noexcept;
};
```

Semantics are unchanged from `gui/DemoPlayer.cpp`: `play` applies keyframe 0 and shows "Flight: {title} (Esc
stops)"; a tick past the total applies the last keyframe and stops; a tick while `!canvasReady()` drops the frame
but the clock keeps running; otherwise `flightSettingsAt(flight, elapsed)` is applied if it changed and the status
shows the percentage; `stop` clears the status and calls `finished`. `MainFrame` owns a `wxTimer` that calls
`tick()`.

### 3.5 `CanvasController`

```cpp
inline constexpr auto   kResizeDebounce         = std::chrono::milliseconds(150);
inline constexpr int    kDragThresholdPx        = 3;
inline constexpr double kWheelZoomPerNotch      = 1.25;
inline constexpr double kKeyZoomFactor          = 2.0;
inline constexpr double kKeyPanFraction         = 0.1;
inline constexpr int    kMaxOrbitPoints         = 512;
inline constexpr int    kOverlayCoordinateLimit = 10000;

enum class CanvasKey : std::uint8_t { LEFT, RIGHT, UP, DOWN, ZOOM_IN, ZOOM_OUT, HOME, ESCAPE };
enum class CanvasCursor : std::uint8_t { CROSS, BULLSEYE };
enum class MouseButton : std::uint8_t { LEFT, RIGHT };

struct RenderStatus { bool rendering{}; std::chrono::milliseconds elapsed{}; int iterations{}; };

/// The fractal view's logic. Every point and size crossing this API is in LOGICAL window pixels; the
/// controller converts to device pixels with the scale it was given (device = logical * scale).
class CanvasController
{
public:
    struct Hooks
    {
        std::function<void(std::function<void()>)> post;  // worker -> UI thread (wx: CallAfter)
        std::function<void()>                      requestRepaint;
        std::function<void(CanvasCursor)>          setCursor;
        std::function<void(bool)>                  captureMouse;  // true = capture, false = release
    };
    CanvasController(RenderSettings initial, Hooks hooks);
    ~CanvasController();  // cancels the render; copy and move deleted

    // Geometry
    void setSize(PixelSize logical, double scale);  // from the size event; no render
    void resizeSettled();                            // after kResizeDebounce: render if the buffer size differs

    // Model (as FractalCanvas today)
    void setSettings(const RenderSettings&);   const RenderSettings& settings() const;
    void setPickSeedMode(bool);                bool pickSeedMode() const;
    void setShowOrbit(bool);                   bool showOrbit() const;
    void resetView();                          void zoomAtCenter(double factor);
    const RgbImage& image() const;             void setHighlighted(bool);
    void showOrbitAt(Complex);                 void clearPinnedOrbit();
    bool hasCoarsePicture() const;             bool rendering() const;
    void cancelRender();

    // Input
    void wheel(PixelPoint at, double notches);
    void buttonDown(MouseButton, PixelPoint at, bool shift);
    void buttonUp(MouseButton, PixelPoint at);
    void pointerMoved(PixelPoint at);
    void pointerLeft();
    void captureLost();
    bool key(CanvasKey);  // false = not handled (the toolkit skips the event); ESCAPE is always handled

    // Paint model
    std::uint64_t              imageVersion() const;  // changes when image() changed; the view rebuilds its bitmap
    PixelPoint                 panOffset() const;
    std::optional<PixelRect>   rubberBand() const;    // normalised like wxRect(p1, p2)
    std::span<const PixelPoint> orbit() const;         // logical coordinates, clamped
    bool                       highlighted() const;

    // Callbacks (set by AppController)
    std::function<void(const ViewSpec&)>                  onViewChanged;
    std::function<void(const std::optional<BigComplex>&)> onPointerMoved;
    std::function<void(Complex)>                          onSeedPicked;
    std::function<void(const RenderStatus&)>              onRenderStatus;
    std::function<void()>                                 onUserInput;
};
```

- Worker callbacks do what `FractalCanvas::startRender` does today, minus wx:
  `auto copy = std::make_shared<TileResult>(tile); m_hooks.post([weak = std::weak_ptr(m_alive), this, copy] { if (weak.lock()) applyTile(*copy); });`
  The weak guard makes a closure drained after destruction a no-op whatever the toolkit's queue does.
- `FractalCanvas` keeps: `wxWindow` creation flags, `SetBackgroundStyle`, the initial `SetCursor(CROSS)`,
  `SetFocus()` on button down, the resize `wxTimer`, key-code mapping onto `CanvasKey`, `Bind()` calls, and
  `onPaint` (bitmap at `panOffset()`, rubber band black solid + white dashed, orbit polyline + red circle,
  highlight ring). Its destructor stays `cancelRender(); DeletePendingEvents();`. About 200 lines remain.

### 3.6 `AppController`

```cpp
enum class StatusField : std::uint8_t { POINTER, CENTER, ZOOM, ITERATIONS, RENDER };
inline constexpr int    kStatusFieldCount = 5;
inline constexpr double kMenuZoomFactor   = 2.0;
enum class GlobalKey : std::uint8_t { ESCAPE, OTHER };

class AppController
{
public:
    /// What only the toolkit can do. The lambdas capture the frame and are invoked only after construction.
    struct Shell
    {
        std::function<void(StatusField, std::string_view)>                    setStatus;
        std::function<void(std::string_view title, std::string_view message)> reportError;
        std::function<void(const RenderSettings&)>                            panelSettings;
        std::function<void(int)>                                              panelEffectiveIterations;
        std::function<void(std::span<const Bookmark>)>                        panelBookmarks;
        std::function<void(bool)>                                             panelPickSeedMode;
        std::function<void(bool)>                                             showOrbitChanged;  // checkbox + menu
        std::function<void(std::optional<Complex>)>                           panelPreviewSeed;
        std::function<void()>                                                 showExportDialog;
        std::function<void()>                                                 closeExportDialog;
        std::function<void()>                                                 raiseWindow;
        std::function<void(std::string_view)>                                 showHelpPage;
        std::function<void(bool)>                                             demoTimer;  // start/stop kDemoTick
        std::function<std::chrono::steady_clock::time_point()>                now;        // optional
        TourScript::Hooks                                                     tour;
    };
    AppController(RenderSettings initial, std::filesystem::path bookmarksPath, CanvasController& canvas,
                  Shell shell);
    ~AppController();  // nulls the canvas callbacks; touches no shell hook
    void start();      // today's MainFrame ctor body: load bookmarks (report errors), refresh list, applySettings

    // Model
    void applySettings(const RenderSettings&);   const RenderSettings& settings() const;
    void setShowOrbit(bool);                     bool showOrbit() const;
    void setPickSeedMode(bool);                  void setPreviewSeed(std::optional<Complex>);
    CanvasController& canvas();                  const BookmarkStore& bookmarks() const;
    const std::filesystem::path& bookmarksPath() const;
    // From the views
    void panelEdited(const RenderSettings& edited);  // "the panel never edits the view"; family/Julia change -> defaultView
    // Menus (the dialogs stay in wx and call these with the result)
    void zoomIn();  void zoomOut();  void resetView();          // each stops a flight first
    void importView(const std::filesystem::path&);  void exportView(const std::filesystem::path&);
    void imageSaved(const std::filesystem::path&);  void imageCopied();
    std::string beginAddBookmark();   // stopDemos(), then returns defaultBookmarkName(settings()) - order matters
    void addBookmark(std::string name);  // "" -> "Untitled"; saves
    void loadBookmark(std::size_t);  void deleteBookmark(std::size_t);  // during the tour: only stops the tour
    // The automation surface (help Try-it links, demos, tour, screenshots)
    void runHelpAction(const HelpAction&);
    void startFlight(std::string_view id);  void startTour();  void stopDemos();  void stopFlight();
    bool flightPlaying() const;  bool tourRunning() const;  TourScript& tour();  void tickDemo();
    void takeSnapshot();  void restoreSnapshot();  bool hasSnapshot() const;
    void addTemporaryBookmark(std::string name);  void removeTemporaryBookmark();
    void showExportDialog();  void closeExportDialog();  void showHelpPage(std::string_view);
    bool handleGlobalKey(GlobalKey);  // Esc while a demo runs -> stopDemos, consumed; any key ends a flight, not consumed

    std::function<void()> onRenderFinished;  // --screenshots mode

private:
    RenderSettings            m_settings;
    BookmarkStore             m_bookmarks;
    CanvasController&         m_canvas;
    Shell                     m_shell;
    DemoPlayer                m_demo;
    std::unique_ptr<TourScript> m_tour;
    std::optional<Snapshot>   m_snapshot;
    std::optional<Bookmark>   m_temporaryBookmark;
};
```

Policies move verbatim from `MainFrame.cpp`: the canvas and panel wiring (L300-355), the status texts
"Centre ...", "Zoom ... (deep)" above the perturbation zoom, "{} iterations", "Rendering...", "Rendered in {} ms",
"Saved {}", "Image copied to clipboard" (L389-427), the pointer field suppressed while a flight plays, the snapshot
rules (L555-573), the temporary bookmark's equality search (L575-600), `runHelpAction` dispatch followed by
`raiseWindow` (L479-521), `addBookmark` stopping demos first (L724).

What remains in `MainFrame` after the cut: the constructor (status bar, menus, `m_canvas`, `m_panel`,
`m_app(initial, path, m_canvas->controller(), makeShell())`, `m_app.start()`), `buildMenus`, `buildHelpMenu`,
`showAbout`, `saveImage` (file dialog -> `exportPngWithProgress` -> `m_app.imageSaved`), `copyImage` (clipboard ->
`m_app.imageCopied`), `exportView`/`importView` (file dialogs -> `m_app.*(path)`), `onAddBookmark`
(`beginAddBookmark` -> text dialog -> `addBookmark`), the export-dialog lifecycle (also the Shell targets),
`showContextHelp` (focus -> `HelpContext` -> `helpPageFor`), `setSidePanelShown`, `onCharHook` ->
`handleGlobalKey` (Skip unless consumed), `onClose`, `startScreenshotRun`. Member order: `m_help`, `m_demoTimer`,
`m_canvas`, `m_panel`, `m_app`, `m_tourView`; `~MainFrame` stops the demo timer. `ScreenshotRun` drives
`frame.app()`.

### 3.7 `TourScript`

```cpp
enum class TourTarget : std::uint8_t { CANVAS, FRACTAL, ITERATIONS, COLOURING, OVERLAY, BOOKMARKS };

struct TourStep
{
    std::string                          title;
    std::string                          text;
    std::string                          helpPage;   // "Learn more"; empty hides the button
    TourTarget                           anchor;     // where the card goes
    std::optional<TourTarget>            highlight;  // nullopt: no ring (the final step)
    std::function<void(AppController&)>  perform;
};

class TourScript
{
public:
    struct Hooks
    {
        std::function<void(const TourStep&, std::size_t index, std::size_t count)> showCard;
        std::function<void()>                                                      hideCard;
        std::function<void(std::optional<TourTarget>)>                             highlight;
    };
    TourScript(AppController& app, Hooks hooks);
    void start();  void stop();  bool running() const;
    void next();   void back();  void showStep(std::size_t index);
    std::size_t stepCount() const;  std::size_t currentStep() const;  const std::vector<TourStep>& steps() const;
};
```

Steps call `AppController` directly, so tests assert real effects ("after step 2 the family is Burning Ship")
rather than a recording. `leaveCurrentStep` = `closeExportDialog` + `canvas().clearPinnedOrbit()`;
`restoreBaseline` = `removeTemporaryBookmark` + `applySettings(baseline)` + `setShowOrbit(baseline)`; `stop()`
keeps the order of `GuidedTour.cpp:218-234`. The wx remainder is renamed `TourView` (`git mv`) and keeps the card,
`anchorFor`, section highlight and scroll, and the frame-resize re-placement.

### 3.8 `ExportTask`

```cpp
inline constexpr int  kExportProgressRange = 1000;
inline constexpr auto kExportPollInterval  = std::chrono::milliseconds(50);

class ExportTask
{
public:
    enum class Outcome : std::uint8_t { RUNNING, SAVED, CANCELLED, FAILED };
    ExportTask(RenderSettings, ExportOptions, std::filesystem::path);  // starts the jthread
    ~ExportTask();                                                      // request_stop + join
    int  progress() const;                    // 0..kExportProgressRange
    bool waitFor(std::chrono::milliseconds);  // true once finished
    void cancel();
    Outcome            outcome() const;
    const std::string& error() const;
};
```

`export_runner.cpp` becomes: create the task, run the `wxProgressDialog` loop
(`while (!task.waitFor(kExportPollInterval)) { if (!dialog.Update(...)) { task.cancel(); dialog.Update(range - 1, "Cancelling..."); } }`),
message box on `FAILED`.

### 3.9 Threading and deterministic tests

- `tests/PostQueue.h`: mutex + condition variable + `std::deque<std::function<void()>>`; `push()` from worker
  threads, `drainAll()`, `waitAndDrainUntil(pred, timeout = 10 s)` on the test thread. It follows the existing
  idiom (`runJob` in `tests/ProgressiveRendererTests.cpp:76-100`: shared_ptr-captured state, a promise, and
  `wait_for` with a timeout; no sleeps). Because nothing mutates controller state until the test drains, states
  like "the shifted placeholder before the first pass lands" are observable deterministically. Tests use tiny
  sizes (`setSize({40, 30}, 1.0)`) so renders take microseconds under asan and tsan.
- `tests/AppHarness.h`: a `RecordingShell` (captures every hook call, a fake clock, the demo-timer flag, the tour
  card and highlight log) and a `Harness { TempDir dir; PostQueue queue; CanvasController canvas; RecordingShell shell; AppController app; }`
  shared by the `AppController` and `TourScript` tests. Its lambdas capture `this`, so it is non-movable.
- `DemoPlayer` has no threads: tests advance the fake clock and call `tick()`. `ExportTask` is polled with a
  deadline.

## 4. Implementation steps

Each step leaves `cmake --workflow --preset dev` green and becomes one commit on `app-layer`.

| # | Step | New / moved | Modified in `src/gui` | Verify |
|---|---|---|---|---|
| 0 | `git switch -c app-layer` from `master` | | | |
| 1 | Scaffold `Mandelbrotter_app`; `format.h`; `git mv` `BookmarkStore` to app | `app/{format,BookmarkStore}.{h,cpp}`, `tests/format_tests.cpp`, `tests/BookmarkStoreTests.cpp` | `src/CMakeLists.txt`, `tests/CMakeLists.txt`, `wx_util.{h,cpp}` (drop the format functions), `MainFrame` includes | dev, tidy |
| 2 | `scenes.h`, `startup.h`; de-duplicate the three copies | `app/{scenes,startup}.{h,cpp}`, `tests/{scenes,startup}_tests.cpp` | `GuidedTour.cpp`, `ScreenshotRun.cpp`, `MandelbrotterApp.{h,cpp}`, `src/main.cpp` | dev, tidy; `--screenshots` still runs |
| 3 | `panel_model.h`, `help_routing.h`, `tour_layout.h`; the views consume them | three headers + `.cpp`, `tests/{panel_model,help_routing,tour_layout}_tests.cpp` | `SidePanel.{h,cpp}` (`Section` alias), `JuliaPreview.cpp`, `TourCard.cpp`, `MainFrame.cpp` (`showContextHelp`), `HelpController.cpp` | dev, tidy |
| 4 | `DemoPlayer` to app (clock and timer hooks) | `app/DemoPlayer.{h,cpp}`, `tests/DemoPlayerTests.cpp`; delete `gui/DemoPlayer.*` | `MainFrame.{h,cpp}` (`wxTimer m_demoTimer`, declared before anything that stops it; stopped in the destructor) | dev, tidy; manual: play a flight, Esc, Help > Demos > Stop demo |
| 5 | `ExportTask`; `export_runner` polls it | `app/ExportTask.{h,cpp}`, `tests/ExportTaskTests.cpp` | `export_runner.cpp` | dev, asan, tsan; manual: save, and cancel a save |
| 6 | `CanvasController`; `FractalCanvas` becomes paint + event translation | `app/CanvasController.{h,cpp}`, `tests/CanvasControllerTests.cpp`, `tests/PostQueue.h` | `FractalCanvas.{h,cpp}`, `MainFrame.cpp`, `GuidedTour.cpp`, `ScreenshotRun.cpp` (`canvas().controller()`) | dev, tidy, asan, tsan; manual navigation smoke |
| 7 | `AppController`; `MainFrame` becomes thin | `app/AppController.{h,cpp}`, `tests/AppControllerTests.cpp`, `tests/AppHarness.h` | `MainFrame.{h,cpp}`, `ScreenshotRun.cpp`, `GuidedTour.cpp`, `SidePanel` wiring | dev, tidy, asan; screenshot diff (section 6) |
| 8 | `TourScript`; `GuidedTour` becomes `TourView` | `app/TourScript.{h,cpp}`, `tests/TourScriptTests.cpp`; `git mv GuidedTour.* TourView.*` | `MainFrame.{h,cpp}`, `ScreenshotRun.cpp` (shot 15: `app().tour().showStep(2)`) | dev, tidy; manual tour: forward, Back, Finish, Close, Esc |
| 9 | Documentation and final verification | | `CLAUDE.md`, `README.md` | all presets, screenshot diff, manual smoke, then push when asked |

## 5. Tests

- **`format_tests`**: `formatZoom` at 1, 5000, the 1e5 boundary, 1.2e6; `formatCenter` decimals =
  `ceil(log10(zoom)) + 6` capped at 16 with "..."; sign of a negative imaginary part; `formatSeedComponent(0.1)`;
  `defaultBookmarkName` -> "Mandelbrot at 5000x".
- **`BookmarkStoreTests`**: missing file -> empty list, no error; save/load round trip in a `TempDir`; garbage
  file -> error string; `remove` out of range is a no-op.
- **`scenes_tests`**: `seahorse()` zoom, centre and palette; `deepSeahorse(z)` centre carries
  `fractionBitsFor(z)` bits; `juliaExample()` is Julia with the seed and default view; `screenshotBookmarks()`
  three names in order; `viewAt` clamps the zoom.
- **`startup_tests`**: `scratchBookmarksPath(tmp)` creates `tmp/Mandelbrotter-screenshots/bookmarks.json` whose
  contents equal `screenshotBookmarks()`; `bookmarksPathFor` picks the user directory without `screenshotsDir`
  and the scratch path with it. Tests never touch the real temp directory (they pass a `TempDir`).
- **`panel_model_tests`**: slider round trips (0, 0.25 -> 250, 1.999 -> 999, negative offsets); `parseSeed`
  trims and fails when either field is bad; `previewSeedFor` in and out of Julia mode; `familyIndex` /
  `paletteIndex` for every entry and an unknown name; `effectiveLimitText`; `previewSettings` has 128 iterations,
  auto off, `defaultView`, the colouring copied.
- **`help_routing_tests`**: a table of (area, section, Julia control) -> page, including NONE -> index,
  CANVAS -> navigating, FRACTAL + Julia -> julia, EXPORT_DIALOG -> exporting; `classifyHelpLink` for
  "mandelbrotter:tour", "https://...", "http://...", "page.html".
- **`tour_layout_tests`**: a wide anchor puts the card inside its top-left corner plus the gap; room on the left
  -> left, top-aligned; otherwise right; clamped into the parent.
- **`DemoPlayerTests`** (fake clock): `play` applies keyframe 0, shows the status and starts the timer; a
  mid-flight tick applies `flightSettingsAt` and shows "NN%"; an identical frame is not re-applied;
  `canvasReady = false` drops the frame but the clock keeps running; at or past the total the last keyframe is
  applied, the status is cleared, `finished` fires and the timer stops; `stop` while idle is a no-op; an empty
  flight is ignored; `play` while playing restarts.
- **`ExportTaskTests`**: SAVED writes a decodable PNG and progress reaches the range; early `cancel` ->
  CANCELLED and no file; an unwritable path -> FAILED with a message; destroying a running task joins.
- **`CanvasControllerTests`** (PostQueue): a wheel notch keeps the complex number under the cursor fixed
  (`Viewport::pixelCenter` before and after) and fires `onUserInput`; a 3 px press/release is a click and 4 px is
  a pan; a right click without a drag zooms out 2x at the pointer; shift-drag and right-drag give `zoomedToRect`
  of the inclusive rectangle; after a pan the image equals the `blitShifted` placeholder and `hasCoarsePicture()`
  is false until the first pass drains, then true; `rubberBand()` is normalised; stale-generation tiles are
  dropped (start A, start B, drain -> buffer equals a `renderSync` of B); the coarse flag flips after all
  first-pass tiles; a palette-only `setSettings` recolours without a new generation; `imageVersion` changes;
  the shallow orbit equals `orbit()` mapped through `toPixel` / `toLocal` and clamped to +-10000; the deep orbit
  uses `ReferenceOrbit` and a deep Julia orbit starts at the pointer; `pointerLeft` clears the orbit unless
  pinned; `showOrbitAt` reports through `onPointerMoved`; the key table (arrows pan 10 %, ZOOM_IN/OUT 2x at the
  centre, HOME resets, ESCAPE cancels a drag and releases capture, ESCAPE is always handled); `captureLost`
  resets the drag and pan offset; pick mode reports `onSeedPicked`, starts no drag and requests the BULLSEYE
  then CROSS cursor; `setSize` followed by `resizeSettled` re-renders only when the size changed; destroying the
  controller with posts pending and then draining does not crash (asan); `onRenderStatus` goes rendering ->
  "Rendered".
- **`AppControllerTests`** (AppHarness): `start()` loads the bookmarks and pushes settings and status;
  `panelEdited` with a family change resets the view and with an exponent change keeps it; a seed pick switches
  to Julia with a default view and turns pick mode off on both the shell and the canvas; a view change from the
  canvas stops a flight and updates CENTER and ZOOM; every status text including "(deep)" above 1e8; the pointer
  text is suppressed while a flight plays; snapshot rules (taken when idle, not overwritten while a demo runs,
  `restoreSnapshot` stops demos and restores settings and orbit, `hasSnapshot` false afterwards); the temporary
  bookmark appears in the shell's list, is removed by equality even after other bookmarks were added, and never
  reaches the file; `beginAddBookmark` during the tour stops it first and returns the baseline's name;
  `addBookmark("")` saves "Untitled"; `deleteBookmark` during the tour only stops the tour and leaves the file
  untouched; `loadBookmark` applies and stops a flight; `runHelpAction` for every variant (view -> snapshot +
  applied, unknown flight -> `reportError`, export -> shell, all -> `raiseWindow`); the `handleGlobalKey` matrix;
  `importView` of a bad file reports an error; `tickDemo` drives a flight through the fake clock and the panel
  catches up at the end; `~AppController` leaves the canvas callbacks empty.
- **`TourScriptTests`**: `stepCount() == 11` (`docs/help/demos.html:47` says eleven); each step's effect
  (0 default view, 1 Seahorse Valley, 2 Burning Ship default view, 3 Julia + preview seed, 4 Seahorse Valley,
  5 fire palette / density 32 / offset 0.25, 6 orbit on and pinned at `kOrbitPoint`, 7 "Tour example" listed,
  8 export dialog shown and closed on leaving, 9 zoom 1e10, 10 baseline restored); `back()` from 7 to 6 re-runs
  step 6 and keeps the bookmark, from 7 to 5 turns the orbit off (fix 1); step 10's highlight is nullopt (fix 2);
  `start()` while running shows step 0 and keeps the baseline; `stop()` restores settings, orbit and the list,
  hides the card and clears the highlight; `next()` on the last step equals `stop()`; `back()` at 0 is a no-op;
  `showStep` when not running is ignored; the card hook receives index, count and whether there is a help page.

## 6. Verification

1. **Per step:** `cmake --workflow --preset dev`. Steps 5-8 also `cmake --workflow --preset tidy`,
   `--preset asan` and `--preset tsan` (tsan is not in CI; it now covers the `CanvasController` and `ExportTask`
   threads). At the end: all four presets, `build/clang-debug/bin/Mandelbrotter --render out.png --size 640x400`,
   and `ctest --preset clang-debug -R 'cli\.|HelpBook|help\.'`.
2. **Screenshot regression** (the strongest end-to-end check the project has). On `master`:
   `cmake --build --preset clang-release`, then
   `GDK_BACKEND=x11 build/clang-release/bin/Mandelbrotter --screenshots <scratch>/before` **twice** (`before`,
   `before2`) to learn which files are stable from run to run. On the branch: the same into `<scratch>/after`.
   `cmp` the stable set byte for byte. Whole-window shots and `ui-status-bar.png` contain "Rendered in N ms",
   which varies; compare those with the status-bar strip chopped off (`magick x.png -gravity South -chop 0x40`).
   Keep the outputs outside `build/`, which is rebuilt on the branch.
3. **Manual smoke of the wx app** (release build): wheel, drag, right-drag, right-click, keys; pick a seed;
   panel edits (a family change resets the view, a bad seed is restored); orbit hover on and off; bookmarks add,
   load, delete; export with cancel; copy image; import and export a view; every flight, Esc, and "Back to where
   I was"; the tour forward, Back, Finish, Close and Esc; F1 on the canvas, each panel section and the export
   dialog; help Try-it links; resize while rendering; close while rendering and while a flight plays.
4. **CI:** `git push -u origin app-layer` only when asked. `ci.yml` runs on pushes to any branch: Linux
   `ci-gcc`, `ci-clang`, `asan`, `tidy` (Arch container), macOS `ci-clang`, Windows `ci-msvc`, and clang-format.
   Any CI failure is reported with a proposed fix before it is implemented. `release.yml` is manual-dispatch only
   and is not run; `VERSION` stays 0.8.3 until a release is decided.

## 7. Behaviour changes

Deliberate and flagged; everything else, including every constant and every text, must be identical.

1. The tour steps set the orbit overlay explicitly (only step 6 turns it on), so leaving step 6 forwards or
   backwards turns it off. Today the overlay stays on until the tour ends.
2. The final tour step ("That is the tour") shows no highlight ring. Today `showStep` re-applies the canvas
   ring right after the step clears it.
3. The unused `formatComplex` is deleted.
4. The wx class `GuidedTour` is renamed `TourView` (its content becomes card, anchor and highlight plumbing; the
   file-equals-class convention).

Kept as they are, being documented behaviour: the tour's temporary bookmark stays until the tour ends (the
step-7 card says so); Finish restores the baseline twice (idempotent, and the second restore covers a user zoom
during the last step).

## 8. Documentation updates

- `CLAUDE.md`, Layout: a new bullet for `src/app/` + `include/Mandelbrotter/app/` (`Mandelbrotter_app`: the
  window's logic without a toolkit; `AppController`, `CanvasController`, `DemoPlayer`, `TourScript`,
  `ExportTask`, `BookmarkStore`, and the scene, format, panel and help-routing helpers; links only the core;
  unit-tested). The gui bullet names `TourView` instead of `GuidedTour` and drops `DemoPlayer`. Conventions: file
  placement for app classes; namespace `mandelbrotter::app`; renderer callbacks are marshalled by
  `CanvasController`'s `post` hook (wx: `CallAfter`); `AppController`'s public methods are the automation surface
  and `MainFrame` keeps only what needs wx; the tests bullet.
- `README.md`, Layout: add the `src/app/` bullet, adjust the gui and tests bullets.
- The help book needs no change: no user-visible number or text changes, and the tour fixes are not described
  there.

## 9. Risks and gotchas

- **clang-tidy on `src/app` is unrelaxed.** Include everything used directly (include-cleaner), no raw `new`
  (owning-memory; `std::unique_ptr<TourScript>` for the lazily created tour), `m_` prefixes on private members,
  `k` prefixes on constants, UPPER_CASE enumerators, `[[nodiscard]]` on const getters, optionals checked before
  `*` (`bugprone-unchecked-optional-access` is on in `src/`), copy and move deleted on classes with a destructor.
- **`-Werror` across compilers:** GCC `-Wuseless-cast` and `-Wdouble-promotion`, `-Wconversion` around
  `lround`; MSVC C4244/C4267 on `size_t` <-> `int` around bookmark indices.
- **Destruction order:** `CanvasController` is a member of `FractalCanvas`; `AppController` is a member of
  `MainFrame`, so it dies before the child windows and its destructor only nulls the canvas callbacks;
  `DemoPlayer::stop` is never called from a destructor (it touches the shell); `m_demoTimer` is declared before
  `m_app` and stopped in `~MainFrame`.
- **Construction order:** `AppController`'s constructor must not call Shell hooks; today's constructor-body work
  (`load`, `refreshBookmarks`, `applySettings`) moves to `start()`, called once the widgets exist.
- `Shell::panelBookmarks(span)` must be consumed synchronously by the toolkit (the store may change afterwards).
  `ProgressiveRenderer::start` cancels synchronously; the stale-tiles test relies on `finishRender` ignoring a
  cancelled completion, as today.
- `ScreenshotRun::finish` deletes `bookmarksPath().parent_path()`; that is only safe because screenshot mode
  always receives the scratch path. The invariant now lives in `startup.h` (`kScratchDirName`); do not pass a
  user path into screenshot mode.
- The formatting hook rewrites every edited C/C++ file; run tidy after edits have settled.
- Out of scope, natural follow-ups once the layer exists: a `MANDELBROTTER_BUILD_GUI` option so core + app +
  tests configure without fetching wxWidgets (and where a Qt front end would be switched on); moving the
  `ScreenshotRun` shot table and its render-wait state machine into the layer; extracting the side panel's
  view-model as a class if a second toolkit finds the pure functions insufficient.
