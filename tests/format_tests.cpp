#include "Mandelbrotter/app/format.h"

#include <gtest/gtest.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace
{

using mandelbrotter::BigComplex;
using mandelbrotter::Complex;
using mandelbrotter::RenderSettings;
using mandelbrotter::app::formatCenter;
using mandelbrotter::app::formatZoom;

BigComplex at(Complex c, double zoom)
{
    return BigComplex::fromComplex(c, mandelbrotter::fractionBitsFor(zoom));
}

TEST(Format, ZoomKeepsSixDigitsBelowOneHundredThousand)
{
    EXPECT_EQ(formatZoom(1.0), "1x");
    EXPECT_EQ(formatZoom(5000.0), "5000x");
    EXPECT_EQ(formatZoom(0.5), "0.5x");
    EXPECT_EQ(formatZoom(99999.0), "99999x");
}

TEST(Format, ZoomUsesThreeSignificantDigitsFromOneHundredThousand)
{
    EXPECT_EQ(formatZoom(1e5), "1e+05x");
    EXPECT_EQ(formatZoom(1.2e6), "1.2e+06x");
    EXPECT_EQ(formatZoom(1e300), "1e+300x");
}

TEST(Format, CenterShowsSixDecimalsBeyondThePixelSize)
{
    // ceil(log10(zoom)) + 6 decimals; zooms below 1 count as 1.
    EXPECT_EQ(formatCenter(at({-0.5, 0.25}, 1.0), 1.0), "-0.500000 + 0.250000i");
    EXPECT_EQ(formatCenter(at({-0.5, 0.25}, 0.1), 0.1), "-0.500000 + 0.250000i");
    EXPECT_EQ(formatCenter(at({-0.7436, 0.1318}, 5000.0), 5000.0), "-0.7436000000 + 0.1318000000i");
}

TEST(Format, CenterCapsTheDecimalsAtSixteen)
{
    // 1e10 wants exactly 16 decimals: no ellipsis yet.
    EXPECT_EQ(formatCenter(at({0.25, 0.5}, 1e10), 1e10),
              "0.2500000000000000 + 0.5000000000000000i");
    // 1e11 wants 17.
    EXPECT_EQ(formatCenter(at({0.25, 0.5}, 1e11), 1e11),
              "0.2500000000000000... + 0.5000000000000000...i");
}

TEST(Format, CenterPutsTheSignOfTheImaginaryPartBetweenTheParts)
{
    EXPECT_EQ(formatCenter(at({0.25, -0.125}, 1.0), 1.0), "0.250000 - 0.125000i");
    EXPECT_EQ(formatCenter(at({-0.25, 0.0}, 1.0), 1.0), "-0.250000 + 0.000000i");
}

TEST(Format, SeedComponentHasTenSignificantDigits)
{
    EXPECT_EQ(mandelbrotter::app::formatSeedComponent(0.1), "0.1");
    EXPECT_EQ(mandelbrotter::app::formatSeedComponent(-0.8), "-0.8");
    EXPECT_EQ(mandelbrotter::app::formatSeedComponent(0.0), "0");
    EXPECT_EQ(mandelbrotter::app::formatSeedComponent(1.0 / 3.0), "0.3333333333");
}

TEST(Format, DefaultBookmarkNameIsTheFamilyAndTheZoom)
{
    RenderSettings settings;
    settings.view.zoom = 5000.0;
    EXPECT_EQ(mandelbrotter::app::defaultBookmarkName(settings), "Mandelbrot at 5000x");
    settings.fractal.family = mandelbrotter::FractalFamily::BURNING_SHIP;
    settings.view.zoom      = 1.2e6;
    EXPECT_EQ(mandelbrotter::app::defaultBookmarkName(settings), "Burning Ship at 1.2e+06x");
}

}  // namespace
