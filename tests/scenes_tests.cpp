#include "Mandelbrotter/app/scenes.h"

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/BigFixed.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/flights.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace
{

using mandelbrotter::BigComplex;
using mandelbrotter::FractalFamily;
using mandelbrotter::RenderSettings;
namespace app = mandelbrotter::app;

/// The precision a center at `zoom` carries: fractionBitsFor rounded up to whole limbs.
int bitsAt(double zoom)
{
    return mandelbrotter::BigFixed(mandelbrotter::fractionBitsFor(zoom)).fractionBits();
}

TEST(Scenes, ViewAtCentersWithThePrecisionOfTheClampedZoom)
{
    const RenderSettings settings = app::viewAt(RenderSettings{}, {0.25, -0.5}, 1e6);
    EXPECT_DOUBLE_EQ(settings.view.zoom, 1e6);
    EXPECT_EQ(settings.view.center.fractionBits(), bitsAt(1e6));
    EXPECT_EQ(settings.view.center, (BigComplex{0.25, -0.5}));

    EXPECT_DOUBLE_EQ(app::viewAt(RenderSettings{}, {}, 1e-9).view.zoom, mandelbrotter::kMinZoom);
    EXPECT_DOUBLE_EQ(app::viewAt(RenderSettings{}, {}, 1e305).view.zoom, mandelbrotter::kMaxZoom);
}

TEST(Scenes, ViewAtKeepsTheRestOfTheBase)
{
    RenderSettings base;
    base.fractal.family           = FractalFamily::TRICORN;
    base.coloring.palette         = "ocean";
    const RenderSettings settings = app::viewAt(base, {0.1, 0.2}, 10.0);
    EXPECT_EQ(settings.fractal, base.fractal);
    EXPECT_EQ(settings.coloring, base.coloring);
}

TEST(Scenes, DefaultsShowTheWholeSet)
{
    const RenderSettings mandelbrot = app::mandelbrotDefault();
    EXPECT_EQ(mandelbrot.fractal, mandelbrotter::FractalSpec{});
    EXPECT_EQ(mandelbrot.view, mandelbrotter::defaultView(mandelbrot.fractal));

    const RenderSettings ship = app::burningShipDefault();
    EXPECT_EQ(ship.fractal.family, FractalFamily::BURNING_SHIP);
    EXPECT_EQ(ship.view, mandelbrotter::defaultView(ship.fractal));
}

TEST(Scenes, JuliaExampleIsTheJuliaSetOfTheTourSeed)
{
    const RenderSettings julia = app::juliaExample();
    EXPECT_TRUE(julia.fractal.julia);
    EXPECT_EQ(julia.fractal.family, FractalFamily::MANDELBROT);
    EXPECT_EQ(julia.fractal.seed, app::kJuliaSeed);
    EXPECT_EQ(julia.view, mandelbrotter::defaultView(julia.fractal));
}

TEST(Scenes, SeahorseIsSeahorseValleyAtZoom5000)
{
    const RenderSettings seahorse = app::seahorse();
    EXPECT_DOUBLE_EQ(seahorse.view.zoom, 5000.0);
    EXPECT_EQ(
        seahorse.view.center,
        BigComplex::fromComplex(app::kSeahorseValley, mandelbrotter::fractionBitsFor(5000.0)));
    EXPECT_EQ(seahorse.coloring.palette, "electric");
    EXPECT_FALSE(seahorse.fractal.julia);
    EXPECT_EQ(app::seahorse("classic").coloring.palette, "classic");
}

TEST(Scenes, SeahorseFireHasADenserShiftedCycle)
{
    const RenderSettings fire = app::seahorseFire();
    EXPECT_EQ(fire.view, app::seahorse().view);
    EXPECT_EQ(fire.coloring.palette, "fire");
    EXPECT_DOUBLE_EQ(fire.coloring.density, 32.0);
    EXPECT_DOUBLE_EQ(fire.coloring.offset, 0.25);
}

TEST(Scenes, DeepSeahorseIsTheDiveDestinationAtTheRequestedZoom)
{
    const mandelbrotter::Flight* dive = mandelbrotter::findFlight("seahorse-dive");
    ASSERT_NE(dive, nullptr);
    const RenderSettings& destination = dive->keyframes.back().settings;
    for (const double zoom : {app::kTourDeepZoom, app::kScreenshotDeepZoom})
    {
        const RenderSettings deep = app::deepSeahorse(zoom);
        EXPECT_DOUBLE_EQ(deep.view.zoom, zoom);
        EXPECT_EQ(deep.view.center.fractionBits(), bitsAt(zoom));
        EXPECT_EQ(deep.view.center,
                  destination.view.center.withFractionBits(mandelbrotter::fractionBitsFor(zoom)));
        EXPECT_EQ(deep.fractal, destination.fractal);
        EXPECT_EQ(deep.coloring, destination.coloring);
    }
}

TEST(Scenes, ScreenshotBookmarksAreThreeNamedPlaces)
{
    const std::vector<mandelbrotter::Bookmark> bookmarks = app::screenshotBookmarks();
    ASSERT_EQ(bookmarks.size(), std::size_t{3});
    EXPECT_EQ(bookmarks[0].name, "Seahorse Valley");
    EXPECT_EQ(bookmarks[1].name, "Elephant Valley");
    EXPECT_EQ(bookmarks[2].name, "The little ship");
    EXPECT_EQ(bookmarks[0].settings.view, app::seahorse().view);
    EXPECT_EQ(bookmarks[0].settings.coloring.palette, "electric");
    EXPECT_EQ(bookmarks[1].settings.coloring.palette, "fire");
    EXPECT_DOUBLE_EQ(bookmarks[1].settings.view.zoom, 60.0);
    EXPECT_EQ(bookmarks[2].settings.fractal.family, FractalFamily::BURNING_SHIP);
    EXPECT_EQ(bookmarks[2].settings.view.center.approx(), (mandelbrotter::Complex{-1.75, -0.03}));
}

}  // namespace
