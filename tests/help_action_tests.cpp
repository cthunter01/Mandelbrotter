#include "Mandelbrotter/help_action.h"

#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace
{

using mandelbrotter::Complex;
using mandelbrotter::ExportDialogAction;
using mandelbrotter::FlightAction;
using mandelbrotter::FractalFamily;
using mandelbrotter::HelpAction;
using mandelbrotter::OrbitAction;
using mandelbrotter::parseHelpAction;
using mandelbrotter::RenderSettings;
using mandelbrotter::ResetAction;
using mandelbrotter::resolveViewAction;
using mandelbrotter::TourAction;
using mandelbrotter::ViewAction;

RenderSettings seahorse()
{
    RenderSettings settings;
    settings.view.zoom   = 5000.0;
    settings.view.center = mandelbrotter::BigComplex::fromComplex(
        {-0.7436, 0.1318}, mandelbrotter::fractionBitsFor(settings.view.zoom));
    return settings;
}

TEST(HelpAction, RecognisesTheScheme)
{
    EXPECT_TRUE(mandelbrotter::isHelpActionUrl("mandelbrotter:tour"));
    EXPECT_TRUE(mandelbrotter::isHelpActionUrl("mandelbrotter:"));
    EXPECT_FALSE(mandelbrotter::isHelpActionUrl("navigating.html"));
    EXPECT_FALSE(mandelbrotter::isHelpActionUrl("https://example.com/mandelbrotter:tour"));
}

TEST(HelpAction, ParsesEveryVerb)
{
    const auto view = parseHelpAction("mandelbrotter:view --zoom 40 --palette fire");
    ASSERT_TRUE(view.has_value()) << view.error();
    EXPECT_EQ(std::get<ViewAction>(*view),
              (ViewAction{.args = {"--zoom", "40", "--palette", "fire"}}));

    const auto flight = parseHelpAction("mandelbrotter:flight seahorse-dive");
    ASSERT_TRUE(flight.has_value()) << flight.error();
    EXPECT_EQ(std::get<FlightAction>(*flight), (FlightAction{.id = "seahorse-dive"}));

    const auto tour = parseHelpAction("mandelbrotter:tour");
    ASSERT_TRUE(tour.has_value()) << tour.error();
    EXPECT_TRUE(std::holds_alternative<TourAction>(*tour));

    const auto orbitOn = parseHelpAction("mandelbrotter:orbit on");
    ASSERT_TRUE(orbitOn.has_value()) << orbitOn.error();
    EXPECT_EQ(std::get<OrbitAction>(*orbitOn), (OrbitAction{.on = true}));
    const auto orbitOff = parseHelpAction("mandelbrotter:orbit off");
    ASSERT_TRUE(orbitOff.has_value()) << orbitOff.error();
    EXPECT_EQ(std::get<OrbitAction>(*orbitOff), (OrbitAction{.on = false}));

    const auto reset = parseHelpAction("mandelbrotter:reset");
    ASSERT_TRUE(reset.has_value()) << reset.error();
    EXPECT_TRUE(std::holds_alternative<ResetAction>(*reset));

    const auto exportDialog = parseHelpAction("mandelbrotter:export");
    ASSERT_TRUE(exportDialog.has_value()) << exportDialog.error();
    EXPECT_TRUE(std::holds_alternative<ExportDialogAction>(*exportDialog));
}

TEST(HelpAction, ToleratesRepeatedSpaces)
{
    const auto view = parseHelpAction("mandelbrotter:view   --zoom \t 40  ");
    ASSERT_TRUE(view.has_value()) << view.error();
    EXPECT_EQ(std::get<ViewAction>(*view), (ViewAction{.args = {"--zoom", "40"}}));
    const auto tour = parseHelpAction("mandelbrotter: tour ");
    ASSERT_TRUE(tour.has_value()) << tour.error();
}

TEST(HelpAction, RejectsMalformedLinks)
{
    const std::vector<std::pair<std::string_view, std::string_view>> cases{
        {"navigating.html", "not a mandelbrotter: link"},
        {"mandelbrotter:", "empty"},
        {"mandelbrotter:fly away", "unknown help action \"fly\""},
        {"mandelbrotter:view", "view needs"},
        {"mandelbrotter:flight", "flight needs"},
        {"mandelbrotter:flight a b", "flight needs"},
        {"mandelbrotter:orbit maybe", "orbit expects"},
        {"mandelbrotter:orbit", "orbit expects"},
        {"mandelbrotter:tour now", "tour takes no arguments"},
        {"mandelbrotter:reset all", "reset takes no arguments"},
        {"mandelbrotter:export png", "export takes no arguments"},
    };
    for (const auto& [url, expected] : cases)
    {
        const auto action = parseHelpAction(url);
        ASSERT_FALSE(action.has_value()) << url;
        EXPECT_NE(action.error().find(expected), std::string::npos) << action.error();
    }
}

TEST(HelpAction, ViewActionKeepsTheCurrentViewForColourChanges)
{
    const RenderSettings current = seahorse();
    const auto           next    = resolveViewAction(current, {.args = {"--palette", "fire"}});
    ASSERT_TRUE(next.has_value()) << next.error();
    EXPECT_EQ(next->view, current.view);
    EXPECT_EQ(next->coloring.palette, "fire");
    EXPECT_EQ(next->fractal, current.fractal);
}

TEST(HelpAction, ViewActionKeepsTheViewWhenOnlyTheExponentChanges)
{
    const RenderSettings current = seahorse();
    const auto           next    = resolveViewAction(current, {.args = {"--exponent", "3"}});
    ASSERT_TRUE(next.has_value()) << next.error();
    EXPECT_EQ(next->view, current.view);
    EXPECT_EQ(next->fractal.exponent, 3);
}

TEST(HelpAction, ViewActionStartsFromTheDefaultViewWhenTheFractalChanges)
{
    const RenderSettings current = seahorse();

    const auto ship = resolveViewAction(current, {.args = {"--fractal", "burning-ship"}});
    ASSERT_TRUE(ship.has_value()) << ship.error();
    EXPECT_EQ(ship->fractal.family, FractalFamily::BURNING_SHIP);
    EXPECT_EQ(ship->view, mandelbrotter::defaultView(ship->fractal));

    const auto julia = resolveViewAction(current, {.args = {"--julia", "-0.8,0.156"}});
    ASSERT_TRUE(julia.has_value()) << julia.error();
    EXPECT_TRUE(julia->fractal.julia);
    EXPECT_EQ(julia->fractal.seed, (Complex{-0.8, 0.156}));
    EXPECT_EQ(julia->view, mandelbrotter::defaultView(julia->fractal));

    // The same family again is not a change: the view stays.
    const auto same = resolveViewAction(current, {.args = {"--fractal", "mandelbrot"}});
    ASSERT_TRUE(same.has_value()) << same.error();
    EXPECT_EQ(same->view, current.view);
}

TEST(HelpAction, ViewActionKeepsAGivenCentreAndZoom)
{
    const auto next = resolveViewAction(
        seahorse(), {.args = {"--fractal", "burning-ship", "--center", "-1.75,-0.03", "--zoom",
                              "40", "--iterations", "500"}});
    ASSERT_TRUE(next.has_value()) << next.error();
    EXPECT_EQ(next->fractal.family, FractalFamily::BURNING_SHIP);
    EXPECT_DOUBLE_EQ(next->view.zoom, 40.0);
    EXPECT_NEAR(next->view.center.approx().re, -1.75, 1e-12);
    EXPECT_NEAR(next->view.center.approx().im, -0.03, 1e-12);
    EXPECT_EQ(next->maxIterations, 500);
    EXPECT_FALSE(next->autoIterations);
}

TEST(HelpAction, ViewActionRejectsOptionsThatAreNotViewOptions)
{
    const std::vector<std::vector<std::string>> cases{
        {"--render", "x.png"}, {"--view", "v.json"},   {"--help"},  {"--screenshots", "dir"},
        {"--size", "10x10"},   {"--supersample", "2"}, {"--bogus"}, {"--zoom", "-1"},
    };
    for (const auto& args : cases)
    {
        const auto next = resolveViewAction(seahorse(), {.args = args});
        EXPECT_FALSE(next.has_value()) << args.front();
    }
}

}  // namespace
