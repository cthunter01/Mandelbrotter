#include "Mandelbrotter/app/ScreenshotScript.h"

#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "AppHarness.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/TourScript.h"
#include "Mandelbrotter/app/panel_model.h"
#include "Mandelbrotter/app/scenes.h"
#include "Mandelbrotter/app/startup.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/geometry.h"

namespace
{

using mandelbrotter::PixelSize;
using mandelbrotter::RenderSettings;
using mandelbrotter::app::PanelSection;
using mandelbrotter::app::ScreenshotScript;
using mandelbrotter::app::ShotRegion;
using mandelbrotter::app::ShotTarget;
using mandelbrotter::test::Harness;
namespace app = mandelbrotter::app;

constexpr PixelSize kPictureSize{800, 500};

/// A toolkit that records what the script asks of it. Its timers never fire by themselves: the
/// test calls settled() when a settle was requested (run()).
struct FakeToolkit
{
    /// The window's state when a picture was taken.
    struct Capture
    {
        ShotTarget            target{};
        ShotRegion            region;
        std::filesystem::path path;
        RenderSettings        settings;
        bool                  showOrbit{};
        bool                  exportDialogOpen{};
        bool                  bookmarkDialogOpen{};
        std::string           bookmarkText;
        bool                  helpOpen{};
        bool                  tourRunning{};
        std::size_t           tourStep{};
    };

    explicit FakeToolkit(Harness& harness) : h(&harness) { }

    [[nodiscard]] ScreenshotScript::Hooks hooks()
    {
        return {
            .setContentSize   = [this](PixelSize size) { contentSizes.push_back(size); },
            .growToWholePanel = [this] { ++grows; },
            .scrollPanelTo    = [this](PanelSection section) { scrolls.push_back(section); },
            .showBookmarkDialog =
                [this](std::string_view text) {
                    bookmarkDialogOpen = true;
                    bookmarkText       = std::string(text);
                },
            .closeBookmarkDialog = [this] { bookmarkDialogOpen = false; },
            .showHelpContents    = [this] { helpOpen = true; },
            .closeHelp           = [this] { helpOpen = false; },
            .refreshAll          = [this] { ++refreshes; },
            .capture =
                [this](ShotTarget target, ShotRegion region,
                       const std::filesystem::path& path) -> std::expected<PixelSize, std::string> {
                captures.push_back({.target             = target,
                                    .region             = region,
                                    .path               = path,
                                    .settings           = h->app.settings(),
                                    .showOrbit          = h->app.showOrbit(),
                                    .exportDialogOpen   = h->shell.exportDialogOpen,
                                    .bookmarkDialogOpen = bookmarkDialogOpen,
                                    .bookmarkText       = bookmarkText,
                                    .helpOpen           = helpOpen,
                                    .tourRunning        = h->app.tourRunning(),
                                    .tourStep           = h->app.tour().currentStep()});
                if (path.filename() == failOn)
                {
                    return std::unexpected("boom");
                }
                return kPictureSize;
            },
            .startSettleTimer =
                [this](std::chrono::milliseconds delay) {
                    EXPECT_EQ(delay, app::kScreenshotSettle);
                    settleRequested = true;
                },
            .startWatchdog =
                [this](std::chrono::milliseconds delay) {
                    EXPECT_EQ(delay, app::kScreenshotWatchdog);
                    ++watchdogStarts;
                },
            .stopTimers = [this] { ++timerStops; },
            .post       = h->queue.hook(),
            .finished =
                [this](int code) {
                    ++finishedCalls;
                    exitCode = code;
                },
            .print      = [this](std::string_view line) { printed.emplace_back(line); },
            .printError = [this](std::string_view line) { errors.emplace_back(line); },
        };
    }

    Harness*                  h;
    std::vector<Capture>      captures;
    std::vector<PixelSize>    contentSizes;
    std::vector<PanelSection> scrolls;
    std::vector<std::string>  printed;
    std::vector<std::string>  errors;
    std::string               bookmarkText;
    std::string               failOn;  ///< a file name whose capture fails
    std::optional<int>        exitCode;
    int                       grows{0};
    int                       refreshes{0};
    int                       watchdogStarts{0};
    int                       timerStops{0};
    int                       finishedCalls{0};
    bool                      settleRequested{false};
    bool                      bookmarkDialogOpen{false};
    bool                      helpOpen{false};
};

/// The toolkit's side of a run until it ends: posted work, and the settle timer when requested.
[[nodiscard]] bool run(Harness& h, ScreenshotScript& script, FakeToolkit& toolkit)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (!script.done())
    {
        if (std::chrono::steady_clock::now() > deadline)
        {
            return false;
        }
        if (toolkit.settleRequested)
        {
            toolkit.settleRequested = false;
            script.settled();
            continue;
        }
        if (!h.queue.waitAndDrainOne())
        {
            return false;
        }
    }
    return true;
}

/// Runs posted work until the script has begun the shot it posted (its watchdog started).
[[nodiscard]] bool beginShot(Harness& h, const FakeToolkit& toolkit, int shot)
{
    return h.queue.waitAndDrainUntil([&] { return toolkit.watchdogStarts == shot; });
}

TEST(ScreenshotScript, ListsTheFifteenPicturesInOrder)
{
    const auto files = ScreenshotScript::files();
    ASSERT_EQ(files.size(), 15U);
    EXPECT_EQ(files.front(), "ui-main-window.png");
    EXPECT_EQ(files.back(), "ui-tour-card.png");
    for (const std::string_view file : files)
    {
        EXPECT_TRUE(file.starts_with("ui-") && file.ends_with(".png")) << file;
    }
}

TEST(ScreenshotScript, TakesEveryShotOfItsTargetAndRegion)
{
    Harness h(app::mandelbrotDefault(), std::string(app::kScratchDirName));
    std::filesystem::create_directories(h.bookmarksFile().parent_path());
    mandelbrotter::saveBookmarks(h.bookmarksFile(), app::screenshotBookmarks());
    h.app.start();
    FakeToolkit      toolkit(h);
    ScreenshotScript script(h.app, h.dir / "shots", toolkit.hooks());
    script.start();
    EXPECT_TRUE(std::filesystem::is_directory(h.dir / "shots"));
    ASSERT_EQ(toolkit.contentSizes.size(), 1U);
    EXPECT_EQ(toolkit.contentSizes[0], app::kScreenshotContentSize);
    EXPECT_TRUE(toolkit.captures.empty());  // the first shot waits for the event loop

    ASSERT_TRUE(run(h, script, toolkit));
    EXPECT_EQ(toolkit.exitCode, 0);
    EXPECT_TRUE(toolkit.errors.empty()) << toolkit.errors.front();

    using Kind         = ShotRegion::Kind;
    const auto whole   = ShotRegion{.kind = Kind::WHOLE, .section = {}};
    const auto section = [](PanelSection which) {
        return ShotRegion{.kind = Kind::SECTION, .section = which};
    };
    const std::vector<std::pair<ShotTarget, ShotRegion>> expected{
        {ShotTarget::MAIN, whole},
        {ShotTarget::MAIN, {.kind = Kind::PANEL, .section = {}}},
        {ShotTarget::MAIN, section(PanelSection::FRACTAL)},
        {ShotTarget::MAIN, section(PanelSection::ITERATIONS)},
        {ShotTarget::MAIN, section(PanelSection::COLORING)},
        {ShotTarget::MAIN, section(PanelSection::OVERLAY)},
        {ShotTarget::MAIN, section(PanelSection::BOOKMARKS)},
        {ShotTarget::MAIN, {.kind = Kind::CANVAS, .section = {}}},
        {ShotTarget::MAIN, {.kind = Kind::CANVAS, .section = {}}},
        {ShotTarget::MAIN, {.kind = Kind::STATUS_BAR, .section = {}}},
        {ShotTarget::MAIN, whole},
        {ShotTarget::EXPORT_DIALOG, whole},
        {ShotTarget::BOOKMARK_DIALOG, whole},
        {ShotTarget::HELP, whole},
        {ShotTarget::MAIN, whole},
    };
    const auto& shots = toolkit.captures;
    ASSERT_EQ(shots.size(), expected.size());
    const auto files = ScreenshotScript::files();
    for (std::size_t i = 0; i < shots.size(); ++i)
    {
        SCOPED_TRACE(files[i]);
        EXPECT_EQ(shots[i].target, expected[i].first);
        EXPECT_EQ(shots[i].region, expected[i].second);
        EXPECT_EQ(shots[i].path, h.dir / "shots" / std::string(files[i]));
        EXPECT_EQ(toolkit.printed.at(i), "  " + std::string(files[i]) + " (800x500)");
    }

    // What each shot shows.
    EXPECT_EQ(shots[0].settings, app::mandelbrotDefault());
    EXPECT_EQ(shots[2].settings, app::juliaExample());
    EXPECT_EQ(shots[3].settings, app::seahorse("classic"));
    EXPECT_EQ(shots[4].settings, app::seahorseFire());
    EXPECT_TRUE(shots[5].showOrbit);
    EXPECT_EQ(shots[7].settings, app::mandelbrotDefault());
    EXPECT_TRUE(shots[7].showOrbit);
    EXPECT_FALSE(shots[8].showOrbit);
    EXPECT_EQ(shots[8].settings, app::seahorse("electric"));
    EXPECT_EQ(shots[9].settings, app::deepSeahorse(app::kScreenshotDeepZoom));
    EXPECT_EQ(shots[10].settings, shots[9].settings);
    EXPECT_TRUE(shots[11].exportDialogOpen);
    EXPECT_FALSE(shots[12].exportDialogOpen);
    EXPECT_TRUE(shots[12].bookmarkDialogOpen);
    EXPECT_EQ(shots[12].bookmarkText, app::kScreenshotBookmarkName);
    EXPECT_FALSE(shots[13].bookmarkDialogOpen);
    EXPECT_TRUE(shots[13].helpOpen);
    EXPECT_FALSE(shots[14].helpOpen);
    EXPECT_TRUE(shots[14].tourRunning);
    EXPECT_EQ(shots[14].tourStep, 2U);

    // The panel's whole-height shot, and the size put back after it.
    EXPECT_EQ(toolkit.grows, 1);
    ASSERT_EQ(toolkit.contentSizes.size(), 2U);
    EXPECT_EQ(toolkit.contentSizes[1], app::kScreenshotContentSize);
    EXPECT_EQ(toolkit.scrolls,
              (std::vector<PanelSection>{PanelSection::FRACTAL, PanelSection::OVERLAY,
                                         PanelSection::BOOKMARKS, PanelSection::FRACTAL}));
    EXPECT_EQ(toolkit.refreshes, 15);
    EXPECT_EQ(toolkit.watchdogStarts, 15);

    // The end: everything put away, the scratch bookmarks deleted, success reported once.
    EXPECT_FALSE(h.app.tourRunning());
    EXPECT_FALSE(h.shell.exportDialogOpen);
    EXPECT_FALSE(toolkit.bookmarkDialogOpen);
    EXPECT_FALSE(h.app.onRenderFinished);
    EXPECT_EQ(toolkit.timerStops, 1);
    EXPECT_EQ(toolkit.finishedCalls, 1);
    EXPECT_FALSE(std::filesystem::exists(h.bookmarksFile().parent_path()));
    ASSERT_EQ(toolkit.printed.size(), 16U);
    EXPECT_EQ(toolkit.printed.back(), "Screenshots written to " + (h.dir / "shots").string());
}

TEST(ScreenshotScript, AShotThatRendersWaitsForItsRender)
{
    Harness h(app::seahorse());
    h.canvas.setSize({800, 600}, 1.0);  // renders that take a while
    h.app.start();
    ASSERT_TRUE(h.finishRender());
    FakeToolkit      toolkit(h);
    ScreenshotScript script(h.app, h.dir / "shots", toolkit.hooks());
    script.start();
    ASSERT_TRUE(beginShot(h, toolkit, 1));  // shows the whole set: a new render
    EXPECT_TRUE(h.canvas.rendering());
    EXPECT_FALSE(toolkit.settleRequested);
    ASSERT_TRUE(h.finishRender());
    EXPECT_TRUE(toolkit.settleRequested);
    EXPECT_EQ(toolkit.refreshes, 1);
}

TEST(ScreenshotScript, WithoutARenderTheShotSettlesAtOnce)
{
    Harness h;
    h.app.start();
    ASSERT_TRUE(h.finishRender());
    FakeToolkit      toolkit(h);
    ScreenshotScript script(h.app, h.dir / "shots", toolkit.hooks());
    script.start();
    ASSERT_TRUE(beginShot(h, toolkit, 1));
    EXPECT_TRUE(toolkit.settleRequested);
}

TEST(ScreenshotScript, TheNextShotComesThroughTheEventLoop)
{
    Harness h;
    h.app.start();
    ASSERT_TRUE(h.finishRender());
    FakeToolkit      toolkit(h);
    ScreenshotScript script(h.app, h.dir / "shots", toolkit.hooks());
    script.start();
    ASSERT_TRUE(beginShot(h, toolkit, 1));
    ASSERT_TRUE(toolkit.settleRequested);
    toolkit.settleRequested = false;
    script.settled();
    EXPECT_EQ(toolkit.captures.size(), 1U);
    EXPECT_EQ(toolkit.watchdogStarts, 1);  // the second shot is posted, not begun
    EXPECT_EQ(toolkit.grows, 0);
    ASSERT_TRUE(beginShot(h, toolkit, 2));
    EXPECT_EQ(toolkit.grows, 1);
}

TEST(ScreenshotScript, ACaptureErrorEndsTheRunAndClosesTheOpenDialog)
{
    Harness h;
    h.app.start();
    FakeToolkit toolkit(h);
    toolkit.failOn = "ui-add-bookmark-dialog.png";
    ScreenshotScript script(h.app, h.dir / "shots", toolkit.hooks());
    script.start();
    ASSERT_TRUE(run(h, script, toolkit));
    EXPECT_EQ(toolkit.exitCode, 1);
    EXPECT_EQ(toolkit.captures.size(), 13U);
    EXPECT_EQ(toolkit.errors,
              std::vector<std::string>{"error: screenshots: ui-add-bookmark-dialog.png: boom"});
    EXPECT_FALSE(toolkit.bookmarkDialogOpen);
    EXPECT_EQ(toolkit.printed.size(), 12U);  // no success line
    EXPECT_FALSE(h.app.onRenderFinished);
}

TEST(ScreenshotScript, TheWatchdogEndsTheRunOnce)
{
    Harness h;
    h.app.start();
    FakeToolkit      toolkit(h);
    ScreenshotScript script(h.app, h.dir / "shots", toolkit.hooks());
    script.start();
    ASSERT_TRUE(beginShot(h, toolkit, 1));
    script.watchdogFired();
    EXPECT_TRUE(script.done());
    EXPECT_EQ(toolkit.exitCode, 1);
    EXPECT_EQ(toolkit.errors, std::vector<std::string>{"error: screenshots: timed out"});
    EXPECT_EQ(toolkit.timerStops, 1);
    EXPECT_FALSE(h.app.onRenderFinished);

    script.watchdogFired();
    script.settled();
    h.queue.drainAll();
    EXPECT_EQ(toolkit.finishedCalls, 1);
    EXPECT_EQ(toolkit.errors.size(), 1U);
    EXPECT_TRUE(toolkit.captures.empty());
}

TEST(ScreenshotScript, OnlyTheScratchDirectoryIsEverDeleted)
{
    Harness h;  // bookmarks in a directory of another name
    mandelbrotter::saveBookmarks(h.bookmarksFile(), app::screenshotBookmarks());
    h.app.start();
    FakeToolkit      toolkit(h);
    ScreenshotScript script(h.app, h.dir / "shots", toolkit.hooks());
    script.start();
    script.watchdogFired();
    EXPECT_TRUE(std::filesystem::exists(h.bookmarksFile()));
}

TEST(ScreenshotScript, DestroyingARunningScriptLetsGoOfTheApplication)
{
    Harness h;
    h.app.start();
    FakeToolkit toolkit(h);
    {
        ScreenshotScript script(h.app, h.dir / "shots", toolkit.hooks());
        script.start();
        EXPECT_TRUE(h.app.onRenderFinished);
    }
    EXPECT_FALSE(h.app.onRenderFinished);
    EXPECT_EQ(toolkit.finishedCalls, 0);
}

}  // namespace
