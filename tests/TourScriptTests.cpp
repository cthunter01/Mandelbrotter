#include "Mandelbrotter/app/TourScript.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "AppHarness.h"
#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/format.h"
#include "Mandelbrotter/app/scenes.h"
#include "Mandelbrotter/bookmarks.h"

namespace
{

using mandelbrotter::Bookmark;
using mandelbrotter::RenderSettings;
using mandelbrotter::app::StatusField;
using mandelbrotter::app::TourTarget;
using mandelbrotter::test::Harness;
namespace app = mandelbrotter::app;

bool listsTourExample(const std::vector<Bookmark>& list)
{
    return std::ranges::any_of(list, [](const Bookmark& b) { return b.name == "Tour example"; });
}

/// The user's state before the tour: fire-colored Seahorse Valley with the overlay on and one
/// bookmark.
struct Started
{
    Started()
    {
        mandelbrotter::saveBookmarks(h.bookmarksFile(), stored);
        h.app.start();
        h.app.setShowOrbit(true);
        h.app.startTour();
    }

    Harness                     h{app::seahorseFire()};
    const std::vector<Bookmark> stored{{.name = "mine", .settings = app::juliaExample()}};
};

TEST(TourScript, HasElevenSteps)
{
    Harness h;
    h.app.start();
    const app::TourScript& tour = h.app.tour();
    ASSERT_EQ(tour.stepCount(), 11U);  // the welcome card and docs/help/demos.html say eleven
    EXPECT_NE(tour.steps().front().text.find("eleven short steps"), std::string::npos);
    EXPECT_FALSE(tour.running());
}

TEST(TourScript, EveryStepHasAPageOfTheHelpBook)
{
    Harness h;
    for (const app::TourStep& step : h.app.tour().steps())
    {
        EXPECT_FALSE(step.title.empty());
        EXPECT_FALSE(step.text.empty());
        EXPECT_TRUE(std::filesystem::is_regular_file(std::filesystem::path(MANDELBROTTER_HELP_DIR) /
                                                     step.helpPage))
            << step.helpPage;
        EXPECT_TRUE(step.perform);
    }
}

TEST(TourScript, StartShowsTheWelcomeCard)
{
    Started  s;
    Harness& h = s.h;
    EXPECT_TRUE(h.app.tourRunning());
    EXPECT_EQ(h.app.tour().currentStep(), 0U);
    ASSERT_EQ(h.shell.cards.size(), 1U);
    EXPECT_EQ(h.shell.cards[0].title, "Welcome to Mandelbrotter");
    EXPECT_EQ(h.shell.cards[0].index, 0U);
    EXPECT_EQ(h.shell.cards[0].count, 11U);
    EXPECT_TRUE(h.shell.cards[0].hasHelpPage);
    EXPECT_EQ(h.shell.cards[0].anchor, TourTarget::CANVAS);
    EXPECT_TRUE(h.shell.cardShown);
    EXPECT_EQ(h.shell.highlights.back(), TourTarget::CANVAS);
    EXPECT_EQ(h.app.settings(), app::mandelbrotDefault());
    EXPECT_FALSE(h.app.showOrbit());
}

TEST(TourScript, EachStepPerformsItsAction)
{
    Started    s;
    Harness&   h    = s.h;
    const auto step = [&h](std::size_t index) {
        h.app.tour().showStep(index);
        EXPECT_EQ(h.shell.cards.back().index, index);
    };

    step(1);
    EXPECT_EQ(h.app.settings(), app::seahorse());
    step(2);
    EXPECT_EQ(h.app.settings(), app::burningShipDefault());
    EXPECT_EQ(h.shell.cards.back().anchor, TourTarget::FRACTAL);
    step(3);
    EXPECT_EQ(h.app.settings(), app::juliaExample());
    EXPECT_EQ(h.shell.previewSeeds.back(), app::kJuliaSeed);
    step(4);
    EXPECT_EQ(h.app.settings(), app::seahorse());
    EXPECT_EQ(h.shell.highlights.back(), TourTarget::ITERATIONS);
    step(5);
    EXPECT_EQ(h.app.settings(), app::seahorseFire());
    EXPECT_EQ(h.shell.highlights.back(), TourTarget::COLORING);
    EXPECT_FALSE(h.app.showOrbit());

    step(6);
    EXPECT_EQ(h.app.settings(), app::mandelbrotDefault());
    EXPECT_TRUE(h.app.showOrbit());
    EXPECT_EQ(h.shell.showOrbit, true);
    EXPECT_FALSE(h.canvas.orbit().empty());  // pinned at the orbit point
    EXPECT_EQ(h.shell.status(StatusField::POINTER),
              app::formatCenter(mandelbrotter::BigComplex::fromComplex(
                                    app::kOrbitPoint, mandelbrotter::fractionBitsFor(1.0)),
                                1.0));
    EXPECT_EQ(h.shell.highlights.back(), TourTarget::OVERLAY);

    step(7);
    EXPECT_EQ(h.app.settings(), app::seahorse());
    EXPECT_FALSE(h.app.showOrbit());
    EXPECT_TRUE(h.canvas.orbit().empty());  // leaving step 6 unpinned it
    EXPECT_EQ(h.shell.status(StatusField::POINTER), "");
    EXPECT_TRUE(listsTourExample(h.shell.bookmarks));
    EXPECT_EQ(h.shell.bookmarks.back().settings, app::seahorse());
    EXPECT_EQ(h.shell.highlights.back(), TourTarget::BOOKMARKS);

    step(8);
    EXPECT_TRUE(h.shell.exportDialogOpen);
    EXPECT_EQ(h.app.settings(), app::seahorse());  // the view from the step before stays
    step(9);
    EXPECT_FALSE(h.shell.exportDialogOpen);  // closed on leaving
    EXPECT_EQ(h.app.settings(), app::deepSeahorse(app::kTourDeepZoom));
    EXPECT_EQ(h.shell.status(StatusField::ZOOM), "Zoom 1e+10x (deep)");
    EXPECT_TRUE(listsTourExample(h.shell.bookmarks));  // the example stays until the end

    step(10);
    EXPECT_EQ(h.app.settings(), app::seahorseFire());  // the baseline, back already
    EXPECT_TRUE(h.app.showOrbit());
    EXPECT_EQ(h.shell.bookmarks, s.stored);
    EXPECT_TRUE(h.app.tourRunning());
    EXPECT_EQ(h.shell.cards.back().title, "That is the tour");
}

TEST(TourScript, TheFinalStepShowsNoHighlight)
{
    Started s;
    s.h.app.tour().showStep(10);
    EXPECT_EQ(s.h.shell.highlights.back(), std::nullopt);
    EXPECT_EQ(s.h.shell.cards.back().anchor, TourTarget::CANVAS);  // the card still has a place
}

TEST(TourScript, BackReRunsThePreviousStep)
{
    Started  s;
    Harness& h = s.h;
    h.app.tour().showStep(7);
    h.app.tour().back();
    EXPECT_EQ(h.app.tour().currentStep(), 6U);
    EXPECT_EQ(h.app.settings(), app::mandelbrotDefault());
    EXPECT_TRUE(h.app.showOrbit());
    EXPECT_FALSE(h.canvas.orbit().empty());
    EXPECT_TRUE(listsTourExample(h.shell.bookmarks));  // the example stays until the end

    h.app.tour().back();  // to step 5: the overlay is off again
    EXPECT_EQ(h.app.tour().currentStep(), 5U);
    EXPECT_FALSE(h.app.showOrbit());
    EXPECT_EQ(h.shell.showOrbit, false);
    EXPECT_TRUE(h.canvas.orbit().empty());
    EXPECT_EQ(h.app.settings(), app::seahorseFire());
}

TEST(TourScript, BackOnTheFirstStepDoesNothing)
{
    Started           s;
    const std::size_t cards = s.h.shell.cards.size();
    s.h.app.tour().back();
    EXPECT_EQ(s.h.shell.cards.size(), cards);
    EXPECT_EQ(s.h.app.tour().currentStep(), 0U);
}

TEST(TourScript, NextWalksToTheEndAndFinishes)
{
    Started  s;
    Harness& h = s.h;
    for (std::size_t i = 1; i < 11; ++i)
    {
        h.app.tour().next();
        EXPECT_EQ(h.app.tour().currentStep(), i);
    }
    EXPECT_TRUE(h.app.tourRunning());
    h.app.tour().next();  // Finish
    EXPECT_FALSE(h.app.tourRunning());
    EXPECT_FALSE(h.shell.cardShown);
    EXPECT_EQ(h.shell.highlights.back(), std::nullopt);
    EXPECT_EQ(h.app.settings(), app::seahorseFire());
    EXPECT_TRUE(h.app.showOrbit());
    EXPECT_EQ(h.shell.bookmarks, s.stored);
    EXPECT_EQ(h.shell.cards.size(), 11U);
}

TEST(TourScript, StopPutsBackWhatTheUserHad)
{
    Started  s;
    Harness& h = s.h;
    h.app.tour().showStep(7);
    h.app.tour().showStep(8);
    h.app.tour().stop();
    EXPECT_FALSE(h.app.tourRunning());
    EXPECT_EQ(h.app.settings(), app::seahorseFire());
    EXPECT_EQ(h.canvas.settings(), app::seahorseFire());
    EXPECT_TRUE(h.app.showOrbit());
    EXPECT_EQ(h.shell.showOrbit, true);
    EXPECT_EQ(h.shell.bookmarks, s.stored);
    EXPECT_EQ(mandelbrotter::loadBookmarks(h.bookmarksFile()), s.stored);
    EXPECT_FALSE(h.shell.exportDialogOpen);
    EXPECT_FALSE(h.shell.cardShown);
    EXPECT_EQ(h.shell.highlights.back(), std::nullopt);
}

TEST(TourScript, StoppingOnTheOrbitStepUnpinsTheOrbit)
{
    Harness h;
    h.app.start();
    h.app.startTour();
    h.app.tour().showStep(6);
    ASSERT_FALSE(h.canvas.orbit().empty());
    h.app.stopDemos();
    EXPECT_FALSE(h.app.showOrbit());
    EXPECT_TRUE(h.canvas.orbit().empty());
    EXPECT_EQ(h.shell.status(StatusField::POINTER), "");
}

TEST(TourScript, StartingAgainGoesBackToTheFirstStepAndKeepsTheBaseline)
{
    Started  s;
    Harness& h = s.h;
    h.app.tour().showStep(4);
    h.app.applySettings(app::juliaExample());  // the user wanders off meanwhile
    h.app.startTour();
    EXPECT_TRUE(h.app.tourRunning());
    EXPECT_EQ(h.app.tour().currentStep(), 0U);
    EXPECT_EQ(h.shell.cards.back().index, 0U);
    h.app.stopDemos();
    EXPECT_EQ(h.app.settings(), app::seahorseFire());
}

TEST(TourScript, NothingHappensWhenNotRunning)
{
    Harness h(app::seahorse());
    h.app.start();
    const int calls = h.shell.calls();
    h.app.tour().showStep(3);
    h.app.tour().next();
    h.app.tour().back();
    h.app.tour().stop();
    EXPECT_EQ(h.shell.calls(), calls);
    EXPECT_EQ(h.app.settings(), app::seahorse());
    EXPECT_TRUE(h.shell.cards.empty());
}

TEST(TourScript, StepsOutOfRangeAreIgnored)
{
    Started           s;
    const std::size_t cards = s.h.shell.cards.size();
    s.h.app.tour().showStep(11);
    EXPECT_EQ(s.h.shell.cards.size(), cards);
    EXPECT_EQ(s.h.app.tour().currentStep(), 0U);
}

TEST(TourScript, AFlightEndsTheTour)
{
    Started s;
    s.h.app.tour().showStep(7);
    s.h.app.startFlight("seahorse-dive");
    EXPECT_FALSE(s.h.app.tourRunning());
    EXPECT_TRUE(s.h.app.flightPlaying());
    EXPECT_EQ(s.h.shell.bookmarks, s.stored);
}

}  // namespace
