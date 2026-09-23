#include "Mandelbrotter/fractal.h"

#include <gtest/gtest.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/geometry.h"

namespace
{

using mandelbrotter::FractalFamily;
using mandelbrotter::FractalSpec;

TEST(Fractal, NamesRoundTrip)
{
    for (const FractalFamily family : mandelbrotter::allFamilies())
    {
        const auto parsed = mandelbrotter::parseFamily(mandelbrotter::toString(family));
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(*parsed, family);
        EXPECT_FALSE(mandelbrotter::displayName(family).empty());
    }
}

TEST(Fractal, ParseAcceptsAliasesAndRejectsUnknown)
{
    EXPECT_EQ(mandelbrotter::parseFamily("burningship"), FractalFamily::BURNING_SHIP);
    EXPECT_EQ(mandelbrotter::parseFamily("burning_ship"), FractalFamily::BURNING_SHIP);
    EXPECT_FALSE(mandelbrotter::parseFamily("Mandelbrot").has_value());
    EXPECT_FALSE(mandelbrotter::parseFamily("").has_value());
}

TEST(Fractal, ExponentIsClamped)
{
    EXPECT_EQ(mandelbrotter::clampExponent(1), mandelbrotter::kMinExponent);
    EXPECT_EQ(mandelbrotter::clampExponent(5), 5);
    EXPECT_EQ(mandelbrotter::clampExponent(100), mandelbrotter::kMaxExponent);
}

TEST(Fractal, DefaultViewsAreValid)
{
    for (const FractalFamily family : mandelbrotter::allFamilies())
    {
        for (const bool julia : {false, true})
        {
            const auto view =
                mandelbrotter::defaultView(FractalSpec{.family = family, .julia = julia});
            EXPECT_GE(view.zoom, mandelbrotter::kMinZoom);
            EXPECT_LE(view.zoom, mandelbrotter::kMaxZoom);
        }
    }
    const auto julia = mandelbrotter::defaultView(FractalSpec{.julia = true});
    EXPECT_EQ(julia.center, (mandelbrotter::BigComplex{0.0, 0.0}));
}

TEST(Fractal, SpecEquality)
{
    const FractalSpec a{
        .family = FractalFamily::TRICORN, .exponent = 3, .julia = true, .seed = {0.1, 0.2}};
    FractalSpec b = a;
    EXPECT_EQ(a, b);
    b.seed.im = 0.3;
    EXPECT_NE(a, b);
}

}  // namespace
