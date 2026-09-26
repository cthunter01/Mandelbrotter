#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/app/BookmarkStore.h"
#include "Mandelbrotter/app/CanvasController.h"
#include "Mandelbrotter/app/DemoPlayer.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/help_action.h"

namespace mandelbrotter::app
{

/// The fields of the status bar, left to right.
enum class StatusField : std::uint8_t
{
    POINTER,  ///< the complex number under the mouse, or a flight's progress
    CENTER,
    ZOOM,
    ITERATIONS,
    RENDER,  ///< render time, or what was just saved or copied
};
inline constexpr int kStatusFieldCount = 5;

/// View > Zoom in / Zoom out.
inline constexpr double kMenuZoomFactor = 2.0;

/// A key pressed anywhere in the main window, before any control sees it.
enum class GlobalKey : std::uint8_t
{
    ESCAPE,
    OTHER,
};

/// The main window's logic, free of any toolkit: the model (RenderSettings) and its bookmarks,
/// the policies that tie the canvas, the side panel and the status bar together, the demos (flights
/// and the guided tour) with their snapshot, and the automation surface the help system drives
/// (Try-it links, the tour, the screenshot mode).
///
/// The toolkit fills a Shell with what only it can do (widgets, dialogs, timers) and routes its
/// events to the public methods. The dialogs themselves stay in the toolkit, which calls in here
/// with their result (a path, a bookmark name).
class AppController
{
public:
    /// What only the toolkit can do. The hooks are called only from start() on, never from the
    /// constructor or the destructor; an empty hook is skipped.
    struct Shell
    {
        std::function<void(StatusField, std::string_view)>                    setStatus;
        std::function<void(std::string_view title, std::string_view message)> reportError;
        /// The side panel shows these settings (it never echoes them back).
        std::function<void(const RenderSettings&)> panelSettings;
        std::function<void(int)>                   panelEffectiveIterations;
        /// The bookmark list; consumed at once (the list may change afterwards).
        std::function<void(std::span<const Bookmark>)> panelBookmarks;
        std::function<void(bool)>                      panelPickSeedMode;
        /// The orbit overlay was switched: its checkbox and menu item follow.
        std::function<void(bool)>                   showOrbitChanged;
        std::function<void(std::optional<Complex>)> panelPreviewSeed;
        /// The Save image as PNG dialog, modeless; showing it again raises it.
        std::function<void()>                 showExportDialog;
        std::function<void()>                 closeExportDialog;
        std::function<void()>                 raiseWindow;
        std::function<void(std::string_view)> showHelpPage;
        /// Start (true) or stop a timer that calls tickDemo() every kDemoTick.
        std::function<void(bool)> demoTimer;
        /// The clock flights run on; steady_clock when empty.
        std::function<std::chrono::steady_clock::time_point()> now;
        /// The guided tour (start restarts it; stop does nothing when it is not running).
        std::function<void()> startTour;
        std::function<void()> stopTour;
        std::function<bool()> tourRunning;
    };

    /// `canvas` must outlive the controller; `bookmarksPath` is the file the bookmarks live in.
    AppController(RenderSettings initial, std::filesystem::path bookmarksPath,
                  CanvasController& canvas, Shell shell);
    /// Disconnects from the canvas; calls no hook.
    ~AppController();
    AppController(const AppController&)            = delete;
    AppController& operator=(const AppController&) = delete;
    AppController(AppController&&)                 = delete;
    AppController& operator=(AppController&&)      = delete;

    /// Once the toolkit's widgets exist: loads the bookmarks (reporting a bad file) and shows the
    /// initial settings everywhere.
    void start();

    // ---- the model
    /// Makes `settings` the current model everywhere (canvas, panel, status bar).
    void                                applySettings(const RenderSettings& settings);
    [[nodiscard]] const RenderSettings& settings() const noexcept { return m_settings; }
    void                                setShowOrbit(bool on);
    [[nodiscard]] bool                  showOrbit() const noexcept { return m_canvas.showOrbit(); }
    void                                setPickSeedMode(bool on);
    /// The seed the Julia preview follows (the panel shows the current seed in Julia mode).
    void                                       setPreviewSeed(std::optional<Complex> seed) const;
    [[nodiscard]] CanvasController&            canvas() noexcept { return m_canvas; }
    [[nodiscard]] const BookmarkStore&         bookmarks() const noexcept { return m_bookmarks; }
    [[nodiscard]] const std::filesystem::path& bookmarksPath() const noexcept
    {
        return m_bookmarks.path();
    }

    // ---- from the views
    /// The side panel's settings changed. The panel never edits the view: it stays, unless the
    /// family or the Julia flag changed, which starts from that fractal's default view.
    void panelEdited(const RenderSettings& edited);

    // ---- menus and buttons (the toolkit runs the dialogs and passes on the result)
    void zoomIn();
    void zoomOut();
    void resetView();
    void importView(const std::filesystem::path& path);
    void exportView(const std::filesystem::path& path);
    void imageSaved(const std::filesystem::path& path);
    void imageCopied();
    /// Add bookmark, before its name dialog: ends the demos (the tour's example bookmark must
    /// never be saved, and the view goes back to the user's), then suggests a name for the view.
    [[nodiscard]] std::string beginAddBookmark();
    /// Adds the current view under `name` ("Untitled" when empty) and saves the file.
    void addBookmark(std::string name);
    void loadBookmark(std::size_t index);
    /// Deletes and saves; during the tour it only ends the tour (the list has changed under the
    /// selection).
    void deleteBookmark(std::size_t index);

    // ---- the help system's automation surface
    void runHelpAction(const HelpAction& action);
    /// Plays a built-in flight (ending the tour); an unknown id is reported.
    void startFlight(std::string_view id);
    void startTour();
    void stopFlight();
    /// Stops a flight and the tour (the tour restores what the user had).
    void               stopDemos();
    [[nodiscard]] bool flightPlaying() const noexcept { return m_demo.playing(); }
    [[nodiscard]] bool tourRunning() const;
    /// The demo timer fired.
    void tickDemo();
    /// Remembers the view for Help > Back to where I was; ignored while a demo already runs.
    void               takeSnapshot();
    void               restoreSnapshot();
    [[nodiscard]] bool hasSnapshot() const noexcept { return m_snapshot.has_value(); }
    /// A bookmark that only appears in the list (the tour's example); never written to disk.
    void addTemporaryBookmark(std::string name);
    void removeTemporaryBookmark();
    void showExportDialog() const;
    void closeExportDialog() const;
    void showHelpPage(std::string_view page) const;
    /// A key pressed anywhere in the window. Esc while a demo runs stops it and is consumed (true);
    /// any other key ends a flight and then does its usual job (false).
    [[nodiscard]] bool handleGlobalKey(GlobalKey key);

    /// Called after every render that ran to completion (the screenshot mode).
    std::function<void()> onRenderFinished;

private:
    struct Snapshot
    {
        RenderSettings settings;
        bool           showOrbit{};
    };

    [[nodiscard]] DemoPlayer::Hooks demoHooks();
    void                            wireCanvas();
    void                            updateStatusBar();
    void                            showPointer(const std::optional<BigComplex>& pointer);
    void                            showRenderStatus(const RenderStatus& status);
    void                            setStatus(StatusField field, std::string_view text) const;
    void reportError(std::string_view title, std::string_view message) const;
    void saveBookmarksFile() const;
    void refreshBookmarks() const;

    RenderSettings          m_settings;
    BookmarkStore           m_bookmarks;
    CanvasController&       m_canvas;
    Shell                   m_shell;
    DemoPlayer              m_demo;  ///< after m_shell: its hooks use the shell's clock
    std::optional<Snapshot> m_snapshot;
    std::optional<Bookmark> m_temporaryBookmark;
};

}  // namespace mandelbrotter::app
