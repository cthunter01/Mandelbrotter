#include "Mandelbrotter/app/AppController.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "AppHarness.h"
#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/app/CanvasController.h"
#include "Mandelbrotter/app/format.h"
#include "Mandelbrotter/app/scenes.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/flights.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/help_action.h"
#include "PostQueue.h"
#include "TempDir.h"

namespace
{

using mandelbrotter::Bookmark;
using mandelbrotter::Complex;
using mandelbrotter::FractalFamily;
using mandelbrotter::RenderSettings;
using mandelbrotter::app::GlobalKey;
using mandelbrotter::app::StatusField;
using mandelbrotter::test::Harness;
namespace app = mandelbrotter::app;

Bookmark named(const std::string& name, const RenderSettings& settings)
{
    return {.name = name, .settings = settings};
}

std::vector<Bookmark> savedBookmarks(const Harness& h)
{
    return mandelbrotter::loadBookmarks(h.bookmarksFile());
}

// ---------------------------------------------------------------------------------------------------------------
// Start-up and the model

TEST(AppController, TheConstructorAndTheDestructorCallNoHook)
{
    const mandelbrotter::test::TempDir dir;
    mandelbrotter::test::PostQueue     queue;
    app::CanvasController              canvas(
        app::mandelbrotDefault(),
        {.post = queue.hook(), .requestRepaint = {}, .setCursor = {}, .captureMouse = {}});
    mandelbrotter::test::RecordingShell shell;
    {
        const app::AppController controller(app::mandelbrotDefault(), dir / "bookmarks.json",
                                            canvas, shell.shell());
        EXPECT_TRUE(canvas.onViewChanged);
        EXPECT_TRUE(canvas.onPointerMoved);
        EXPECT_TRUE(canvas.onSeedPicked);
        EXPECT_TRUE(canvas.onRenderStatus);
        EXPECT_TRUE(canvas.onUserInput);
    }
    EXPECT_EQ(shell.calls(), 0);
    // Destroyed, it has let go of the canvas.
    EXPECT_FALSE(canvas.onViewChanged);
    EXPECT_FALSE(canvas.onPointerMoved);
    EXPECT_FALSE(canvas.onSeedPicked);
    EXPECT_FALSE(canvas.onRenderStatus);
    EXPECT_FALSE(canvas.onUserInput);
}

TEST(AppController, StartLoadsTheBookmarksAndShowsTheSettings)
{
    Harness                     h(app::seahorse());
    const std::vector<Bookmark> stored{named("one", app::mandelbrotDefault()),
                                       named("two", app::juliaExample())};
    mandelbrotter::saveBookmarks(h.bookmarksFile(), stored);
    h.app.start();

    EXPECT_TRUE(h.shell.errors.empty());
    EXPECT_EQ(h.shell.bookmarks, stored);
    EXPECT_EQ(h.app.bookmarks().bookmarks(), stored);
    EXPECT_EQ(h.app.bookmarksPath(), h.bookmarksFile());
    ASSERT_FALSE(h.shell.panelSettings.empty());
    EXPECT_EQ(h.shell.panelSettings.back(), app::seahorse());
    EXPECT_EQ(h.canvas.settings(), app::seahorse());
    EXPECT_EQ(h.shell.status(StatusField::CENTER), "Center -0.7436000000 + 0.1318000000i");
    EXPECT_EQ(h.shell.status(StatusField::ZOOM), "Zoom 5000x");
    const int iterations = mandelbrotter::effectiveIterations(app::seahorse());
    EXPECT_EQ(h.shell.status(StatusField::ITERATIONS), std::to_string(iterations) + " iterations");
    EXPECT_EQ(h.shell.effectiveIterations, iterations);
    EXPECT_EQ(h.shell.status(StatusField::RENDER), "Rendering...");
    ASSERT_TRUE(h.finishRender());
    EXPECT_TRUE(h.shell.status(StatusField::RENDER).starts_with("Rendered in "));
    EXPECT_TRUE(h.shell.status(StatusField::RENDER).ends_with(" ms"));
}

TEST(AppController, AnUnreadableBookmarksFileIsReported)
{
    Harness h;
    std::ofstream(h.bookmarksFile()) << "not json";
    h.app.start();
    ASSERT_EQ(h.shell.errors.size(), 1U);
    EXPECT_EQ(h.shell.errors[0].title, "Bookmarks");
    EXPECT_TRUE(h.shell.errors[0].message.starts_with("Could not read " +
                                                      h.bookmarksFile().string() + ":\n"));
    EXPECT_TRUE(h.shell.bookmarks.empty());
    EXPECT_EQ(h.shell.panelSettings.back(), app::mandelbrotDefault());  // the window still opens
}

TEST(AppController, ApplySettingsReachesEveryView)
{
    Harness h;
    h.app.start();
    const RenderSettings julia = app::juliaExample();
    h.app.applySettings(julia);
    EXPECT_EQ(h.app.settings(), julia);
    EXPECT_EQ(h.canvas.settings(), julia);
    EXPECT_EQ(h.shell.panelSettings.back(), julia);
    EXPECT_EQ(h.shell.status(StatusField::ZOOM), "Zoom 0.75x");
}

TEST(AppController, TheStatusBarMarksDeepZooms)
{
    Harness h;
    h.app.start();
    RenderSettings settings = app::viewAt(app::mandelbrotDefault(), {-0.75, 0.1}, 1e8);
    h.app.applySettings(settings);
    EXPECT_EQ(h.shell.status(StatusField::ZOOM), "Zoom 1e+08x");
    settings = app::viewAt(settings, {-0.75, 0.1}, 2e10);
    h.app.applySettings(settings);
    EXPECT_EQ(h.shell.status(StatusField::ZOOM), "Zoom 2e+10x (deep)");
    EXPECT_EQ(h.shell.status(StatusField::CENTER),
              "Center -0.7500000000000000... + 0.1000000000000000...i");
}

TEST(AppController, TheOrbitSwitchReachesTheCanvasAndTheShell)
{
    Harness h;
    h.app.start();
    h.app.setShowOrbit(true);
    EXPECT_TRUE(h.app.showOrbit());
    EXPECT_TRUE(h.canvas.showOrbit());
    EXPECT_EQ(h.shell.showOrbit, true);
    h.app.setShowOrbit(false);
    EXPECT_FALSE(h.canvas.showOrbit());
    EXPECT_EQ(h.shell.showOrbit, false);
}

// ---------------------------------------------------------------------------------------------------------------
// The panel and the canvas

TEST(AppController, APanelEditKeepsTheView)
{
    Harness h(app::seahorse());
    h.app.start();
    RenderSettings edited   = app::seahorse();
    edited.fractal.exponent = 3;
    edited.view             = app::mandelbrotDefault().view;  // the panel's stale copy
    h.app.panelEdited(edited);
    EXPECT_EQ(h.app.settings().fractal.exponent, 3);
    EXPECT_EQ(h.app.settings().view, app::seahorse().view);
    EXPECT_EQ(h.canvas.settings(), h.app.settings());
}

TEST(AppController, AFamilyOrJuliaChangeStartsFromTheDefaultView)
{
    Harness h(app::seahorse());
    h.app.start();
    RenderSettings edited = app::seahorse();
    edited.fractal.family = FractalFamily::TRICORN;
    h.app.panelEdited(edited);
    EXPECT_EQ(h.app.settings().fractal.family, FractalFamily::TRICORN);
    EXPECT_EQ(h.app.settings().view, mandelbrotter::defaultView(edited.fractal));

    h.app.applySettings(app::seahorse());
    edited               = app::seahorse();
    edited.fractal.julia = true;
    edited.fractal.seed  = app::kJuliaSeed;
    h.app.panelEdited(edited);
    EXPECT_EQ(h.app.settings().view, mandelbrotter::defaultView(edited.fractal));
    EXPECT_EQ(h.shell.panelSettings.back(), h.app.settings());
}

TEST(AppController, APickedSeedOpensItsJuliaSet)
{
    Harness h(app::seahorse());
    h.app.start();
    h.app.setPickSeedMode(true);
    EXPECT_EQ(h.shell.pickSeedMode, true);
    EXPECT_TRUE(h.canvas.pickSeedMode());

    const Complex expected = mandelbrotter::Viewport(h.canvas.settings().view, Harness::kCanvasSize)
                                 .pixelCenter({10, 7});
    h.canvas.buttonDown(app::MouseButton::LEFT, {10, 7}, false);
    EXPECT_TRUE(h.app.settings().fractal.julia);
    EXPECT_EQ(h.app.settings().fractal.seed, expected);
    EXPECT_EQ(h.app.settings().view, mandelbrotter::defaultView(h.app.settings().fractal));
    EXPECT_EQ(h.app.settings().coloring, app::seahorse().coloring);
    EXPECT_EQ(h.shell.pickSeedMode, false);
    EXPECT_FALSE(h.canvas.pickSeedMode());
    EXPECT_EQ(h.shell.panelSettings.back(), h.app.settings());
}

TEST(AppController, NavigatingUpdatesThePanelAndTheStatusBar)
{
    Harness h;
    h.app.start();
    const std::size_t pushes = h.shell.panelSettings.size();
    h.canvas.key(app::CanvasKey::ZOOM_IN);
    EXPECT_DOUBLE_EQ(h.app.settings().view.zoom, 2.0);
    EXPECT_EQ(h.shell.status(StatusField::ZOOM), "Zoom 2x");
    EXPECT_EQ(h.shell.status(StatusField::CENTER),
              "Center " + app::formatCenter(h.canvas.settings().view.center, 2.0));
    ASSERT_EQ(h.shell.panelSettings.size(), pushes + 1);
    EXPECT_EQ(h.shell.panelSettings.back(), h.app.settings());
}

TEST(AppController, MouseInputAndViewChangesStopAFlight)
{
    Harness h;
    h.app.start();
    h.app.startFlight("seahorse-dive");
    ASSERT_TRUE(h.app.flightPlaying());
    h.canvas.buttonDown(app::MouseButton::LEFT, {5, 5}, false);  // no view change yet
    EXPECT_FALSE(h.app.flightPlaying());

    h.app.startFlight("seahorse-dive");
    h.canvas.key(app::CanvasKey::LEFT);  // a view change
    EXPECT_FALSE(h.app.flightPlaying());
}

TEST(AppController, ThePointerFieldFollowsTheMouseUnlessAFlightPlays)
{
    Harness h;
    h.app.start();
    h.canvas.pointerMoved({10, 7});
    const mandelbrotter::BigComplex pointer =
        mandelbrotter::Viewport(h.canvas.settings().view, Harness::kCanvasSize)
            .pixelCenterBig({10, 7});
    EXPECT_EQ(h.shell.status(StatusField::POINTER), app::formatCenter(pointer, 1.0));
    ASSERT_FALSE(h.shell.previewSeeds.empty());
    EXPECT_EQ(h.shell.previewSeeds.back(), pointer.approx());

    h.canvas.pointerLeft();
    EXPECT_EQ(h.shell.status(StatusField::POINTER), "");
    EXPECT_EQ(h.shell.previewSeeds.back(), std::nullopt);

    h.app.startFlight("seahorse-dive");
    EXPECT_EQ(h.shell.status(StatusField::POINTER),
              "Flight: Dive into Seahorse Valley (Esc stops)");
    h.canvas.pointerMoved({10, 7});
    EXPECT_EQ(h.shell.status(StatusField::POINTER),
              "Flight: Dive into Seahorse Valley (Esc stops)");
    EXPECT_TRUE(h.shell.previewSeeds.back().has_value());  // the preview still follows
}

TEST(AppController, TheRenderFieldReportsRendersSavesAndCopies)
{
    Harness h;
    int     finished       = 0;
    h.app.onRenderFinished = [&] { ++finished; };
    h.app.start();
    EXPECT_EQ(h.shell.status(StatusField::RENDER), "Rendering...");
    ASSERT_TRUE(h.finishRender());
    EXPECT_EQ(finished, 1);
    h.app.imageSaved(h.dir / "picture.png");
    EXPECT_EQ(h.shell.status(StatusField::RENDER), "Saved picture.png");
    h.app.imageCopied();
    EXPECT_EQ(h.shell.status(StatusField::RENDER), "Image copied to clipboard");
}

// ---------------------------------------------------------------------------------------------------------------
// Menus

TEST(AppController, MenuZoomAndResetStopAFlightFirst)
{
    Harness h;
    h.app.start();
    h.app.zoomIn();
    EXPECT_DOUBLE_EQ(h.app.settings().view.zoom, 2.0);
    h.app.zoomIn();
    h.app.zoomOut();
    EXPECT_DOUBLE_EQ(h.app.settings().view.zoom, 2.0);

    h.app.startFlight("seahorse-dive");
    h.app.resetView();
    EXPECT_FALSE(h.app.flightPlaying());
    EXPECT_EQ(h.app.settings().view, mandelbrotter::defaultView(h.app.settings().fractal));
}

TEST(AppController, ViewsExportAndImport)
{
    Harness h(app::seahorseFire());
    h.app.start();
    h.app.exportView(h.dir / "view.json");
    EXPECT_TRUE(h.shell.errors.empty());

    h.app.applySettings(app::mandelbrotDefault());
    h.app.startFlight("seahorse-dive");
    h.app.importView(h.dir / "view.json");
    EXPECT_FALSE(h.app.flightPlaying());
    EXPECT_EQ(h.app.settings(), app::seahorseFire());
    EXPECT_EQ(h.shell.panelSettings.back(), app::seahorseFire());
}

TEST(AppController, BadViewFilesAreReported)
{
    Harness h;
    h.app.start();
    std::ofstream(h.dir / "bad.json") << "{ nope";
    h.app.importView(h.dir / "bad.json");
    h.app.importView(h.dir / "missing.json");
    std::ofstream(h.dir / "file") << "not a directory";
    h.app.exportView(h.dir / "file" / "view.json");
    ASSERT_EQ(h.shell.errors.size(), 3U);
    EXPECT_EQ(h.shell.errors[0].title, "Import view");
    EXPECT_EQ(h.shell.errors[1].title, "Import view");
    EXPECT_EQ(h.shell.errors[2].title, "Export view");
    EXPECT_EQ(h.app.settings(), app::mandelbrotDefault());
}

// ---------------------------------------------------------------------------------------------------------------
// Bookmarks

TEST(AppController, AddingABookmarkSavesTheView)
{
    Harness h(app::seahorse());
    h.app.start();
    EXPECT_EQ(h.app.beginAddBookmark(), "Mandelbrot at 5000x");
    h.app.addBookmark("Seahorses");
    h.app.addBookmark("");
    const std::vector<Bookmark> expected{named("Seahorses", app::seahorse()),
                                         named("Untitled", app::seahorse())};
    EXPECT_EQ(savedBookmarks(h), expected);
    EXPECT_EQ(h.shell.bookmarks, expected);
}

TEST(AppController, BeginningABookmarkEndsTheDemos)
{
    Harness h(app::seahorse());
    h.app.start();
    h.app.startFlight("seahorse-dive");
    EXPECT_EQ(h.app.beginAddBookmark(), app::defaultBookmarkName(h.app.settings()));
    EXPECT_FALSE(h.app.flightPlaying());  // the flight stops where it is

    h.app.applySettings(app::seahorse());
    h.app.startTour();
    h.app.tour().showStep(7);  // lists the tour's example bookmark
    h.app.tour().showStep(9);  // and moves the view deep
    EXPECT_EQ(h.app.bookmarks().bookmarks().size(), 1U);
    // The tour ends first: the name is for the user's own view, and the example is gone.
    EXPECT_EQ(h.app.beginAddBookmark(), "Mandelbrot at 5000x");
    EXPECT_FALSE(h.app.tourRunning());
    h.app.addBookmark("mine");
    EXPECT_EQ(savedBookmarks(h), std::vector<Bookmark>{named("mine", app::seahorse())});
}

TEST(AppController, LoadingABookmarkAppliesItAndStopsAFlight)
{
    Harness h;
    mandelbrotter::saveBookmarks(h.bookmarksFile(),
                                 std::vector<Bookmark>{named("julia", app::juliaExample())});
    h.app.start();
    h.app.startFlight("seahorse-dive");
    h.app.loadBookmark(0);
    EXPECT_FALSE(h.app.flightPlaying());
    EXPECT_EQ(h.app.settings(), app::juliaExample());

    h.app.applySettings(app::seahorse());
    h.app.loadBookmark(5);  // no such bookmark
    EXPECT_EQ(h.app.settings(), app::seahorse());
}

TEST(AppController, DeletingABookmarkSavesTheList)
{
    Harness h;
    mandelbrotter::saveBookmarks(
        h.bookmarksFile(),
        std::vector<Bookmark>{named("a", app::seahorse()), named("b", app::juliaExample())});
    h.app.start();
    h.app.deleteBookmark(0);
    const std::vector<Bookmark> expected{named("b", app::juliaExample())};
    EXPECT_EQ(savedBookmarks(h), expected);
    EXPECT_EQ(h.shell.bookmarks, expected);
}

TEST(AppController, DeletingDuringTheTourOnlyEndsTheTour)
{
    Harness                     h;
    const std::vector<Bookmark> stored{named("a", app::seahorse())};
    mandelbrotter::saveBookmarks(h.bookmarksFile(), stored);
    h.app.start();
    h.app.startTour();
    h.app.tour().showStep(7);
    ASSERT_EQ(h.shell.bookmarks.size(), 2U);
    h.app.deleteBookmark(0);  // the selection may be stale: nothing is deleted
    EXPECT_FALSE(h.app.tourRunning());
    EXPECT_EQ(savedBookmarks(h), stored);
    EXPECT_EQ(h.app.bookmarks().bookmarks(), stored);
    EXPECT_EQ(h.shell.bookmarks, stored);
}

TEST(AppController, AnUnwritableBookmarksFileIsReported)
{
    const mandelbrotter::test::TempDir dir;
    mandelbrotter::test::PostQueue     queue;
    std::ofstream(dir / "file") << "not a directory";
    app::CanvasController canvas(
        app::mandelbrotDefault(),
        {.post = queue.hook(), .requestRepaint = {}, .setCursor = {}, .captureMouse = {}});
    mandelbrotter::test::RecordingShell shell;
    app::AppController controller(app::mandelbrotDefault(), dir / "file" / "bookmarks.json", canvas,
                                  shell.shell());
    controller.start();
    controller.addBookmark("x");
    ASSERT_EQ(shell.errors.size(), 1U);
    EXPECT_EQ(shell.errors[0].title, "Bookmarks");
    EXPECT_TRUE(shell.errors[0].message.starts_with("Could not write "));
    EXPECT_EQ(shell.bookmarks.size(), 1U);  // kept in the list all the same
}

TEST(AppController, TheTemporaryBookmarkIsListedButNeverSaved)
{
    Harness                     h(app::seahorse());
    const std::vector<Bookmark> stored{named("a", app::juliaExample())};
    mandelbrotter::saveBookmarks(h.bookmarksFile(), stored);
    h.app.start();
    h.app.addTemporaryBookmark("Tour example");
    ASSERT_EQ(h.shell.bookmarks.size(), 2U);
    EXPECT_EQ(h.shell.bookmarks[1], named("Tour example", app::seahorse()));
    EXPECT_EQ(savedBookmarks(h), stored);

    h.app.addTemporaryBookmark("Tour example");  // replaces the first
    EXPECT_EQ(h.shell.bookmarks.size(), 2U);
    h.app.removeTemporaryBookmark();
    EXPECT_EQ(h.shell.bookmarks, stored);
    h.app.removeTemporaryBookmark();  // nothing left to remove
    EXPECT_EQ(h.shell.bookmarks, stored);
}

TEST(AppController, TheTemporaryBookmarkIsFoundByEquality)
{
    // The user already has a bookmark equal to the tour's example: the example (the last one) goes.
    Harness                     h(app::seahorse());
    const std::vector<Bookmark> stored{named("Tour example", app::seahorse()),
                                       named("b", app::juliaExample())};
    mandelbrotter::saveBookmarks(h.bookmarksFile(), stored);
    h.app.start();
    h.app.addTemporaryBookmark("Tour example");
    h.app.applySettings(app::mandelbrotDefault());  // the view moves on; the example stays
    ASSERT_EQ(h.app.bookmarks().bookmarks().size(), 3U);
    h.app.removeTemporaryBookmark();
    EXPECT_EQ(h.app.bookmarks().bookmarks(), stored);
}

// ---------------------------------------------------------------------------------------------------------------
// Demos and the snapshot

TEST(AppController, AFlightPlaysOnTheDemoClock)
{
    Harness h;
    h.app.start();
    ASSERT_TRUE(h.finishRender());
    const std::size_t            pushes = h.shell.panelSettings.size();
    const mandelbrotter::Flight* flight = mandelbrotter::findFlight("palette-sweep");
    ASSERT_NE(flight, nullptr);
    h.app.startFlight("palette-sweep");
    EXPECT_TRUE(h.shell.demoTimer);
    EXPECT_EQ(h.app.settings(), flight->keyframes.front().settings);
    EXPECT_EQ(h.canvas.settings(), flight->keyframes.front().settings);

    ASSERT_TRUE(h.coarsePicture());
    h.shell.advance(std::chrono::seconds(3));
    h.app.tickDemo();
    EXPECT_EQ(h.app.settings(), mandelbrotter::flightSettingsAt(*flight, std::chrono::seconds(3)));
    EXPECT_EQ(h.shell.status(StatusField::POINTER),
              "Flight: " + flight->title + " 25% (Esc stops)");
    EXPECT_EQ(h.shell.panelSettings.size(), pushes);  // the panel waits for the end

    h.shell.advance(mandelbrotter::totalDuration(*flight));
    h.app.tickDemo();
    EXPECT_FALSE(h.app.flightPlaying());
    EXPECT_FALSE(h.shell.demoTimer);
    EXPECT_EQ(h.app.settings(), flight->keyframes.back().settings);
    EXPECT_EQ(h.shell.panelSettings.back(), flight->keyframes.back().settings);
    EXPECT_EQ(h.shell.status(StatusField::POINTER), "");
}

TEST(AppController, AnUnknownFlightIsReported)
{
    Harness h;
    h.app.start();
    h.app.startFlight("nope");
    ASSERT_EQ(h.shell.errors.size(), 1U);
    EXPECT_EQ(h.shell.errors[0].title, "Demos");
    EXPECT_EQ(h.shell.errors[0].message, "There is no flight called \"nope\".");
    EXPECT_FALSE(h.app.flightPlaying());
}

TEST(AppController, FlightsAndTheTourStopEachOther)
{
    Harness h;
    h.app.start();
    h.app.startFlight("seahorse-dive");
    h.app.startTour();
    EXPECT_FALSE(h.app.flightPlaying());
    EXPECT_TRUE(h.app.tourRunning());
    h.app.startFlight("seahorse-dive");
    EXPECT_FALSE(h.app.tourRunning());
    EXPECT_TRUE(h.app.flightPlaying());
    h.app.startTour();
    h.app.stopDemos();
    EXPECT_FALSE(h.app.flightPlaying());
    EXPECT_FALSE(h.app.tourRunning());
}

TEST(AppController, TheSnapshotKeepsTheViewFromBeforeTheDemos)
{
    Harness h(app::seahorse());
    h.app.start();
    EXPECT_FALSE(h.app.hasSnapshot());
    h.app.restoreSnapshot();  // nothing to restore
    EXPECT_EQ(h.app.settings(), app::seahorse());

    h.app.setShowOrbit(true);
    h.app.takeSnapshot();
    h.app.setShowOrbit(false);
    h.app.startFlight("seahorse-dive");
    h.app.takeSnapshot();  // a demo runs: the first snapshot stays
    h.app.restoreSnapshot();
    EXPECT_FALSE(h.app.flightPlaying());
    EXPECT_EQ(h.app.settings(), app::seahorse());
    EXPECT_TRUE(h.app.showOrbit());
    EXPECT_EQ(h.shell.showOrbit, true);
    EXPECT_FALSE(h.app.hasSnapshot());
}

TEST(AppController, TheSnapshotIsRetakenWhenIdle)
{
    Harness h(app::seahorse());
    h.app.start();
    h.app.takeSnapshot();
    h.app.applySettings(app::juliaExample());
    h.app.takeSnapshot();  // nothing runs: this view replaces the first
    h.app.applySettings(app::mandelbrotDefault());
    h.app.restoreSnapshot();
    EXPECT_EQ(h.app.settings(), app::juliaExample());
}

TEST(AppController, EscapeStopsDemosAndOtherKeysEndFlights)
{
    Harness h;
    h.app.start();
    EXPECT_FALSE(h.app.handleGlobalKey(GlobalKey::ESCAPE));  // nothing runs: Esc goes on
    EXPECT_FALSE(h.app.handleGlobalKey(GlobalKey::OTHER));

    h.app.startFlight("seahorse-dive");
    EXPECT_TRUE(h.app.handleGlobalKey(GlobalKey::ESCAPE));
    EXPECT_FALSE(h.app.flightPlaying());

    h.app.startFlight("seahorse-dive");
    EXPECT_FALSE(h.app.handleGlobalKey(GlobalKey::OTHER));  // stops, then does its job
    EXPECT_FALSE(h.app.flightPlaying());

    h.app.startTour();
    EXPECT_FALSE(h.app.handleGlobalKey(GlobalKey::OTHER));  // the tour lets keys through
    EXPECT_TRUE(h.app.tourRunning());
    EXPECT_TRUE(h.app.handleGlobalKey(GlobalKey::ESCAPE));
    EXPECT_FALSE(h.app.tourRunning());
}

// ---------------------------------------------------------------------------------------------------------------
// Help actions

TEST(AppController, AViewActionAppliesTheOptions)
{
    Harness h;
    h.app.start();
    h.app.startFlight("seahorse-dive");
    const RenderSettings before = h.app.settings();
    h.app.runHelpAction(mandelbrotter::ViewAction{{"--zoom", "5000", "--palette", "fire"}});
    EXPECT_FALSE(h.app.flightPlaying());
    EXPECT_DOUBLE_EQ(h.app.settings().view.zoom, 5000.0);
    EXPECT_EQ(h.app.settings().coloring.palette, "fire");
    EXPECT_EQ(h.shell.raises, 1);
    ASSERT_TRUE(h.app.hasSnapshot());
    h.app.restoreSnapshot();
    EXPECT_EQ(h.app.settings(), before);
}

TEST(AppController, ABadViewActionIsReported)
{
    Harness h;
    h.app.start();
    h.app.runHelpAction(mandelbrotter::ViewAction{{"--zoom"}});
    ASSERT_EQ(h.shell.errors.size(), 1U);
    EXPECT_EQ(h.shell.errors[0].title, "Try it");
    EXPECT_EQ(h.shell.raises, 0);
    EXPECT_FALSE(h.app.hasSnapshot());
    EXPECT_EQ(h.app.settings(), app::mandelbrotDefault());
}

TEST(AppController, EveryOtherActionRaisesTheWindow)
{
    Harness h;
    h.app.start();
    h.app.runHelpAction(mandelbrotter::FlightAction{"seahorse-dive"});
    EXPECT_TRUE(h.app.flightPlaying());
    EXPECT_TRUE(h.app.hasSnapshot());
    EXPECT_EQ(h.shell.raises, 1);

    h.app.runHelpAction(mandelbrotter::FlightAction{"nope"});
    EXPECT_EQ(h.shell.errors.size(), 1U);
    EXPECT_EQ(h.shell.raises, 2);

    h.app.runHelpAction(mandelbrotter::TourAction{});
    EXPECT_TRUE(h.app.tourRunning());
    EXPECT_EQ(h.shell.raises, 3);

    h.app.stopDemos();
    h.app.runHelpAction(mandelbrotter::OrbitAction{.on = true});
    EXPECT_TRUE(h.app.showOrbit());
    EXPECT_EQ(h.shell.raises, 4);

    h.app.zoomIn();
    h.app.runHelpAction(mandelbrotter::ResetAction{});
    EXPECT_EQ(h.app.settings().view, mandelbrotter::defaultView(h.app.settings().fractal));
    EXPECT_EQ(h.shell.raises, 5);

    h.app.runHelpAction(mandelbrotter::ExportDialogAction{});
    EXPECT_TRUE(h.shell.exportDialogOpen);
    EXPECT_EQ(h.shell.raises, 6);
}

TEST(AppController, HelpPagesAndTheExportDialogGoToTheShell)
{
    Harness h;
    h.app.start();
    h.app.showHelpPage("julia.html");
    EXPECT_EQ(h.shell.helpPages, std::vector<std::string>{"julia.html"});
    h.app.showExportDialog();
    EXPECT_TRUE(h.shell.exportDialogOpen);
    h.app.closeExportDialog();
    EXPECT_FALSE(h.shell.exportDialogOpen);
    h.app.setPreviewSeed(Complex{0.1, 0.2});
    EXPECT_EQ(h.shell.previewSeeds.back(), (Complex{0.1, 0.2}));
}

}  // namespace
