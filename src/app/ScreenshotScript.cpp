#include "Mandelbrotter/app/ScreenshotScript.h"

#include <array>
#include <chrono>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/CanvasController.h"
#include "Mandelbrotter/app/TourScript.h"
#include "Mandelbrotter/app/panel_model.h"
#include "Mandelbrotter/app/scenes.h"
#include "Mandelbrotter/app/startup.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::app
{

namespace
{

constexpr std::array<std::string_view, 15> kFiles{
    "ui-main-window.png",         "ui-side-panel.png",       "ui-fractal-section.png",
    "ui-iterations-section.png",  "ui-coloring-section.png", "ui-overlay-section.png",
    "ui-bookmarks-section.png",   "ui-orbit-overlay.png",    "ui-canvas-seahorse.png",
    "ui-status-bar.png",          "ui-deep-zoom.png",        "ui-export-dialog.png",
    "ui-add-bookmark-dialog.png", "ui-help-window.png",      "ui-tour-card.png",
};

/// Calls a hook that may be empty.
template <typename Signature, typename... Args>
void call(const std::function<Signature>& hook, Args&&... args)
{
    if (hook)
    {
        hook(std::forward<Args>(args)...);
    }
}

ShotRegion section(PanelSection which)
{
    return {.kind = ShotRegion::Kind::SECTION, .section = which};
}

ShotRegion region(ShotRegion::Kind kind)
{
    return {.kind = kind, .section = {}};
}

}  // namespace

ScreenshotScript::ScreenshotScript(AppController& app, std::filesystem::path dir, Hooks hooks)
  : m_app(app), m_dir(std::move(dir)), m_hooks(std::move(hooks)), m_shots(buildShots())
{
}

ScreenshotScript::~ScreenshotScript()
{
    if (!m_done)
    {
        m_app.onRenderFinished = nullptr;
    }
}

std::span<const std::string_view> ScreenshotScript::files() noexcept
{
    return kFiles;
}

std::vector<ScreenshotScript::Shot> ScreenshotScript::buildShots()
{
    using Kind          = ShotRegion::Kind;
    const auto nothing  = [] { };
    const auto scrollTo = [this](PanelSection which) {
        return [this, which] { call(m_hooks.scrollPanelTo, which); };
    };
    std::vector<Shot> shots{
        {
            .file         = kFiles[0],
            .rendersFirst = true,
            .target       = ShotTarget::MAIN,
            .region       = region(Kind::WHOLE),
            .prepare      = [this] { m_app.applySettings(mandelbrotDefault()); },
            .cleanup      = nothing,
        },
        {
            .file         = kFiles[1],
            .rendersFirst = true,
            .target       = ShotTarget::MAIN,
            .region       = region(Kind::PANEL),
            .prepare =
                [this] {
                    // Tall enough for the whole panel (normally it scrolls).
                    call(m_hooks.growToWholePanel);
                    call(m_hooks.scrollPanelTo, PanelSection::FRACTAL);
                },
            .cleanup = [this] { call(m_hooks.setContentSize, kScreenshotContentSize); },
        },
        {
            .file         = kFiles[2],
            .rendersFirst = true,
            .target       = ShotTarget::MAIN,
            .region       = section(PanelSection::FRACTAL),
            .prepare =
                [this] {
                    m_app.applySettings(juliaExample());
                    m_app.setPreviewSeed(kJuliaSeed);
                },
            .cleanup = nothing,
        },
        {
            .file         = kFiles[3],
            .rendersFirst = true,
            .target       = ShotTarget::MAIN,
            .region       = section(PanelSection::ITERATIONS),
            .prepare      = [this] { m_app.applySettings(seahorse("classic")); },
            .cleanup      = nothing,
        },
        {
            .file         = kFiles[4],
            .rendersFirst = false,
            .target       = ShotTarget::MAIN,
            .region       = section(PanelSection::COLORING),
            .prepare      = [this] { m_app.applySettings(seahorseFire()); },
            .cleanup      = nothing,
        },
        {
            .file         = kFiles[5],
            .rendersFirst = false,
            .target       = ShotTarget::MAIN,
            .region       = section(PanelSection::OVERLAY),
            .prepare =
                [this, scrollTo] {
                    m_app.setShowOrbit(true);
                    scrollTo(PanelSection::OVERLAY)();
                },
            .cleanup = nothing,
        },
        {
            .file         = kFiles[6],
            .rendersFirst = false,
            .target       = ShotTarget::MAIN,
            .region       = section(PanelSection::BOOKMARKS),
            .prepare      = scrollTo(PanelSection::BOOKMARKS),  // the scratch bookmarks
            .cleanup      = nothing,
        },
        {
            .file         = kFiles[7],
            .rendersFirst = true,
            .target       = ShotTarget::MAIN,
            .region       = region(Kind::CANVAS),
            .prepare =
                [this] {
                    m_app.applySettings(mandelbrotDefault());
                    m_app.setShowOrbit(true);
                    m_app.canvas().showOrbitAt(kOrbitPoint);
                },
            .cleanup = [this] { m_app.canvas().clearPinnedOrbit(); },
        },
        {
            .file         = kFiles[8],
            .rendersFirst = true,
            .target       = ShotTarget::MAIN,
            .region       = region(Kind::CANVAS),
            .prepare =
                [this] {
                    m_app.setShowOrbit(false);
                    m_app.applySettings(seahorse("electric"));
                },
            .cleanup = nothing,
        },
        {
            .file         = kFiles[9],
            .rendersFirst = true,
            .target       = ShotTarget::MAIN,
            .region       = region(Kind::STATUS_BAR),
            .prepare =
                [this, scrollTo] {
                    scrollTo(PanelSection::FRACTAL)();
                    m_app.applySettings(deepSeahorse(kScreenshotDeepZoom));
                },
            .cleanup = nothing,
        },
        {
            .file         = kFiles[10],  // keeps the deep zoom of the status bar's shot
            .rendersFirst = false,
            .target       = ShotTarget::MAIN,
            .region       = region(Kind::WHOLE),
            .prepare      = nothing,
            .cleanup      = nothing,
        },
        {
            .file         = kFiles[11],
            .rendersFirst = false,
            .target       = ShotTarget::EXPORT_DIALOG,
            .region       = region(Kind::WHOLE),
            .prepare      = [this] { m_app.showExportDialog(); },
            .cleanup      = [this] { m_app.closeExportDialog(); },
        },
        {
            .file         = kFiles[12],
            .rendersFirst = false,
            .target       = ShotTarget::BOOKMARK_DIALOG,
            .region       = region(Kind::WHOLE),
            .prepare =
                [this] {
                    m_bookmarkDialogOpen = true;
                    call(m_hooks.showBookmarkDialog, kScreenshotBookmarkName);
                },
            .cleanup = [this] { closeBookmarkDialog(); },
        },
        {
            .file         = kFiles[13],
            .rendersFirst = false,
            .target       = ShotTarget::HELP,
            .region       = region(Kind::WHOLE),
            .prepare      = [this] { call(m_hooks.showHelpContents); },
            .cleanup      = [this] { call(m_hooks.closeHelp); },
        },
        {
            .file         = kFiles[14],
            .rendersFirst = true,
            .target       = ShotTarget::MAIN,
            .region       = region(Kind::WHOLE),
            .prepare =
                [this] {
                    m_app.startTour();
                    m_app.tour().showStep(
                        2);  // the Families step: card beside a highlighted section
                },
            .cleanup = [this] { m_app.stopDemos(); },
        },
    };
    return shots;
}

void ScreenshotScript::start()
{
    std::filesystem::create_directories(m_dir);
    call(m_hooks.setContentSize, kScreenshotContentSize);
    m_app.onRenderFinished = [this] { renderFinished(); };
    // Let the window map and lay itself out before the first shot.
    call(m_hooks.post, [this] { runCurrent(); });
}

void ScreenshotScript::runCurrent()
{
    if (m_done)
    {
        return;
    }
    if (m_index >= m_shots.size())
    {
        finish(0);
        return;
    }
    const Shot& shot = m_shots[m_index];
    call(m_hooks.startWatchdog, std::chrono::milliseconds(kScreenshotWatchdog));
    shot.prepare();
    m_waitingForRender = shot.rendersFirst && m_app.canvas().rendering();
    if (!m_waitingForRender)
    {
        settle();
    }
}

void ScreenshotScript::renderFinished()
{
    if (m_waitingForRender)
    {
        m_waitingForRender = false;
        settle();
    }
}

void ScreenshotScript::settle() const
{
    // A full repaint first (after a long render GTK on X11 can leave the panel's static box frames
    // undrawn until the next one); the pause then lets the toolkit paint.
    call(m_hooks.refreshAll);
    call(m_hooks.startSettleTimer, kScreenshotSettle);
}

void ScreenshotScript::settled()
{
    if (m_done || m_index >= m_shots.size() || m_waitingForRender)
    {
        return;
    }
    const Shot& shot = m_shots[m_index];
    if (!m_hooks.capture)
    {
        fail(std::format("{}: nothing can capture the window", shot.file));
        return;
    }
    const std::expected<PixelSize, std::string> captured =
        m_hooks.capture(shot.target, shot.region, m_dir / shot.file);
    if (!captured)
    {
        fail(std::format("{}: {}", shot.file, captured.error()));
        return;
    }
    call(m_hooks.print, std::format("  {} ({}x{})", shot.file, captured->width, captured->height));
    shot.cleanup();
    ++m_index;
    call(m_hooks.post, [this] { runCurrent(); });
}

void ScreenshotScript::watchdogFired()
{
    if (!m_done)
    {
        fail("timed out");
    }
}

void ScreenshotScript::fail(std::string_view why)
{
    call(m_hooks.printError, std::format("error: screenshots: {}", why));
    finish(1);
}

void ScreenshotScript::closeBookmarkDialog()
{
    if (m_bookmarkDialogOpen)
    {
        m_bookmarkDialogOpen = false;
        call(m_hooks.closeBookmarkDialog);
    }
}

void ScreenshotScript::finish(int exitCode)
{
    if (m_done)
    {
        return;
    }
    m_done = true;
    call(m_hooks.stopTimers);
    m_app.onRenderFinished = nullptr;
    closeBookmarkDialog();
    // The scratch bookmarks the application created for this run (bookmarksPathFor). Checked by
    // name, so a run that was handed any other bookmarks file never deletes its directory.
    const std::filesystem::path scratch = m_app.bookmarksPath().parent_path();
    if (scratch.filename() == kScratchDirName)
    {
        std::error_code ignored;
        std::filesystem::remove_all(scratch, ignored);
    }
    if (exitCode == 0)
    {
        call(m_hooks.print, std::format("Screenshots written to {}", m_dir.string()));
    }
    call(m_hooks.finished, exitCode);
}

}  // namespace mandelbrotter::app
