#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Mandelbrotter/app/panel_model.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::app
{

class AppController;

/// The numbers of the developer screenshot mode (--screenshots DIR).
inline constexpr auto kScreenshotSettle   = std::chrono::milliseconds(500);  ///< repaint pause
inline constexpr auto kScreenshotWatchdog = std::chrono::seconds(120);       ///< per shot
/// The window's content (without the menu and status bars) while the shots are taken.
inline constexpr PixelSize kScreenshotContentSize{1000, 640};
inline constexpr int       kScreenshotPanelSlack = 16;   ///< below the whole panel (its shot)
inline constexpr int       kScreenshotMaxWidth   = 800;  ///< wider pictures are scaled down
inline constexpr int       kScreenshotMarginPx   = 4;    ///< around a cropped region
/// The text of the Add bookmark dialog's shot.
inline constexpr std::string_view kScreenshotBookmarkName = "Mandelbrot at 1x";

/// The window a shot captures.
enum class ShotTarget : std::uint8_t
{
    MAIN,
    EXPORT_DIALOG,
    BOOKMARK_DIALOG,
    HELP,
};

/// The part of the target a shot keeps.
struct ShotRegion
{
    enum class Kind : std::uint8_t
    {
        WHOLE,
        PANEL,
        SECTION,
        CANVAS,
        STATUS_BAR,
    };
    Kind         kind{Kind::WHOLE};
    PanelSection section{};  ///< SECTION: which box

    bool operator==(const ShotRegion&) const = default;
};

/// The developer screenshot mode without a toolkit: walks the window through the states the help
/// book shows (the ui-*.png pictures), has the toolkit capture each one after the canvas has
/// finished rendering and a pause for the toolkit to repaint, then ends. Failures are printed and
/// end the run with exit code 1; a watchdog ends a shot that takes too long.
///
/// Everything runs on the toolkit's thread: the toolkit calls settled() and watchdogFired() from
/// its timers, and post() runs a closure later on its event loop.
class ScreenshotScript
{
public:
    struct Hooks
    {
        /// The window's content size (wx: SetClientSize; the bars come on top).
        std::function<void(PixelSize content)> setContentSize;
        /// kScreenshotContentSize's width, and tall enough for the whole side panel plus
        /// kScreenshotPanelSlack (at least kScreenshotContentSize's height).
        std::function<void()>                      growToWholePanel;
        std::function<void(PanelSection)>          scrollPanelTo;
        std::function<void(std::string_view text)> showBookmarkDialog;  ///< modeless
        std::function<void()>                      closeBookmarkDialog;
        std::function<void()>                      showHelpContents;
        std::function<void()>                      closeHelp;
        std::function<void()>                      refreshAll;
        /// Raises the target and takes its picture: fails on a missing window or an all-black
        /// picture, crops the region with kScreenshotMarginPx around it, undoes the pixel ratio,
        /// caps the width at kScreenshotMaxWidth and writes a PNG. Returns the picture's size.
        std::function<std::expected<PixelSize, std::string>(ShotTarget, ShotRegion,
                                                            const std::filesystem::path&)>
                                                       capture;
        std::function<void(std::chrono::milliseconds)> startSettleTimer;  ///< then settled()
        std::function<void(std::chrono::milliseconds)> startWatchdog;     ///< then watchdogFired()
        std::function<void()>                          stopTimers;
        std::function<void(std::function<void()>)>     post;
        /// The run is over: the toolkit keeps the exit code and closes the window.
        std::function<void(int exitCode)>     finished;
        std::function<void(std::string_view)> print;       ///< a line for stdout
        std::function<void(std::string_view)> printError;  ///< a line for stderr
    };

    /// `app` must outlive the script; the shots are written into `dir`.
    ScreenshotScript(AppController& app, std::filesystem::path dir, Hooks hooks);
    ScreenshotScript(const ScreenshotScript&)            = delete;
    ScreenshotScript& operator=(const ScreenshotScript&) = delete;
    ScreenshotScript(ScreenshotScript&&)                 = delete;
    ScreenshotScript& operator=(ScreenshotScript&&)      = delete;
    ~ScreenshotScript();

    /// Creates the directory, sets the window's size, follows the renders and posts the first
    /// shot (the window lays itself out first).
    void start();
    /// The settle timer fired: the shot is captured.
    void settled();
    /// The watchdog fired: the run fails.
    void watchdogFired();

    [[nodiscard]] bool done() const noexcept { return m_done; }
    /// The pictures the run writes, in order.
    [[nodiscard]] static std::span<const std::string_view> files() noexcept;

private:
    struct Shot
    {
        std::string_view      file;
        bool                  rendersFirst{};  ///< wait for the canvas after prepare
        ShotTarget            target{ShotTarget::MAIN};
        ShotRegion            region;
        std::function<void()> prepare;
        std::function<void()> cleanup;
    };

    [[nodiscard]] std::vector<Shot> buildShots();
    void                            runCurrent();
    void                            renderFinished();
    void                            settle() const;
    void                            fail(std::string_view why);
    void                            finish(int exitCode);
    void                            closeBookmarkDialog();

    AppController&        m_app;
    std::filesystem::path m_dir;
    Hooks                 m_hooks;
    std::vector<Shot>     m_shots;
    std::size_t           m_index{0};
    bool                  m_waitingForRender{false};
    bool                  m_bookmarkDialogOpen{false};
    bool                  m_done{false};
};

}  // namespace mandelbrotter::app
