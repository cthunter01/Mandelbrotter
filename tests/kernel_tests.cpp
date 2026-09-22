#include "Mandelbrotter/kernel.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace
{

using mandelbrotter::Complex;
using mandelbrotter::FractalFamily;
using mandelbrotter::FractalSpec;
using mandelbrotter::IterationResult;

constexpr int         kMaxIter = 1000;
constexpr FractalSpec kMandelbrot{};

TEST(Kernel, OriginIsInterior)
{
    const IterationResult r = mandelbrotter::iteratePixel(kMandelbrot, {0.0, 0.0}, kMaxIter);
    EXPECT_TRUE(r.interior);
    EXPECT_DOUBLE_EQ(r.smoothIter, kMaxIter);
}

TEST(Kernel, MinusTwoIsInteriorAndBeyondIsNot)
{
    EXPECT_TRUE(mandelbrotter::iteratePixel(kMandelbrot, {-2.0, 0.0}, kMaxIter).interior);
    EXPECT_FALSE(mandelbrotter::iteratePixel(kMandelbrot, {-2.01, 0.0}, kMaxIter).interior);
}

TEST(Kernel, OneEscapesAfterFiveSteps)
{
    // 0 -> 1 -> 2 -> 5 -> 26 -> 677: |z5| = 677 > 256, detected before the sixth step.
    const IterationResult r = mandelbrotter::iteratePixel(kMandelbrot, {1.0, 0.0}, kMaxIter);
    EXPECT_FALSE(r.interior);
    EXPECT_GT(r.smoothIter, 4.0);
    EXPECT_LE(r.smoothIter, 5.0);
}

TEST(Kernel, CuspIsInteriorJustOutsideEscapesSlowly)
{
    EXPECT_TRUE(mandelbrotter::iteratePixel(kMandelbrot, {0.25, 0.0}, kMaxIter).interior);
    const IterationResult r = mandelbrotter::iteratePixel(kMandelbrot, {0.26, 0.0}, kMaxIter);
    EXPECT_FALSE(r.interior);
    EXPECT_GT(r.smoothIter, 20.0);
    EXPECT_LT(r.smoothIter, 60.0);
}

TEST(Kernel, MaxIterZeroIsInteriorUnlessStartAlreadyEscaped)
{
    EXPECT_TRUE(mandelbrotter::iteratePixel(kMandelbrot, {1.0, 0.0}, 0).interior);
    const FractalSpec     julia{.julia = true, .seed = {0.0, 0.0}};
    const IterationResult r = mandelbrotter::iteratePixel(julia, {1000.0, 0.0}, 0);
    EXPECT_FALSE(r.interior);
    EXPECT_DOUBLE_EQ(r.smoothIter, 0.0);
}

TEST(Kernel, SmoothCountIsContinuousAlongTheRealAxis)
{
    // Escape time decreases smoothly from c = 0.35 to c = 1: consecutive samples never jump by a
    // whole step.
    constexpr int kSamples = 2000;
    double previous = mandelbrotter::iteratePixel(kMandelbrot, {0.35, 0.0}, kMaxIter).smoothIter;
    double maxDelta = 0.0;
    for (int i = 1; i <= kSamples; ++i)
    {
        const double c = 0.35 + (0.65 * i) / kSamples;
        const double current =
            mandelbrotter::iteratePixel(kMandelbrot, {c, 0.0}, kMaxIter).smoothIter;
        maxDelta = std::max(maxDelta, std::abs(current - previous));
        EXPECT_LE(current, previous + 1e-9) << "escape time must not increase at c = " << c;
        previous = current;
    }
    EXPECT_LT(maxDelta, 0.05);
}

TEST(Kernel, MandelbrotAndTricornAreConjugateSymmetric)
{
    for (const FractalFamily family : {FractalFamily::Mandelbrot, FractalFamily::Tricorn})
    {
        const FractalSpec spec{.family = family};
        for (const Complex c :
             {Complex{-0.7, 0.3}, Complex{0.3, 0.5}, Complex{-1.2, 0.1}, Complex{0.1, 0.9}})
        {
            const IterationResult a = mandelbrotter::iteratePixel(spec, c, kMaxIter);
            const IterationResult b = mandelbrotter::iteratePixel(spec, {c.re, -c.im}, kMaxIter);
            EXPECT_EQ(a, b) << mandelbrotter::toString(family);
        }
    }
}

TEST(Kernel, FamiliesDiffer)
{
    // Over a small grid of points, each pair of families disagrees somewhere.
    int mandelbrotVsShip    = 0;
    int mandelbrotVsTricorn = 0;
    int shipVsTricorn       = 0;
    for (int i = 0; i <= 8; ++i)
    {
        for (int j = 0; j <= 8; ++j)
        {
            const Complex         c{-1.5 + (0.25 * i), -1.0 + (0.25 * j)};
            const IterationResult m =
                mandelbrotter::iteratePixel({.family = FractalFamily::Mandelbrot}, c, kMaxIter);
            const IterationResult s =
                mandelbrotter::iteratePixel({.family = FractalFamily::BurningShip}, c, kMaxIter);
            const IterationResult t =
                mandelbrotter::iteratePixel({.family = FractalFamily::Tricorn}, c, kMaxIter);
            mandelbrotVsShip += m != s ? 1 : 0;
            mandelbrotVsTricorn += m != t ? 1 : 0;
            shipVsTricorn += s != t ? 1 : 0;
        }
    }
    EXPECT_GT(mandelbrotVsShip, 5);
    EXPECT_GT(mandelbrotVsTricorn, 5);
    EXPECT_GT(shipVsTricorn, 5);
}

TEST(Kernel, BurningShipBasics)
{
    const FractalSpec ship{.family = FractalFamily::BurningShip};
    EXPECT_TRUE(mandelbrotter::iteratePixel(ship, {0.0, 0.0}, kMaxIter).interior);
    EXPECT_TRUE(mandelbrotter::iteratePixel(ship, {-1.0, 0.0}, kMaxIter).interior);
    EXPECT_FALSE(mandelbrotter::iteratePixel(ship, {2.0, 2.0}, kMaxIter).interior);
}

TEST(Kernel, HigherExponentsBehave)
{
    for (int n = 3; n <= mandelbrotter::kMaxExponent; ++n)
    {
        const FractalSpec spec{.exponent = n};
        EXPECT_TRUE(mandelbrotter::iteratePixel(spec, {0.0, 0.0}, kMaxIter).interior) << n;
        // 0 -> 2 -> 2^n + 2 -> ... escapes within a few steps.
        const IterationResult r = mandelbrotter::iteratePixel(spec, {2.0, 0.0}, kMaxIter);
        EXPECT_FALSE(r.interior) << n;
        EXPECT_GT(r.smoothIter, 0.0) << n;
        EXPECT_LT(r.smoothIter, 4.0) << n;
        // The unit disc boundary at |c| = 1 escapes for every n (|z| grows once above 1).
        EXPECT_FALSE(mandelbrotter::iteratePixel(spec, {1.1, 0.0}, kMaxIter).interior) << n;
    }
}

TEST(Kernel, ExponentOutOfRangeIsClamped)
{
    const IterationResult low = mandelbrotter::iteratePixel({.exponent = 1}, {0.3, 0.4}, kMaxIter);
    const IterationResult two = mandelbrotter::iteratePixel({.exponent = 2}, {0.3, 0.4}, kMaxIter);
    EXPECT_EQ(low, two);
    const IterationResult high =
        mandelbrotter::iteratePixel({.exponent = 50}, {0.9, 0.4}, kMaxIter);
    const IterationResult eight =
        mandelbrotter::iteratePixel({.exponent = 8}, {0.9, 0.4}, kMaxIter);
    EXPECT_EQ(high, eight);
}

TEST(Kernel, JuliaModeUsesPixelAsStartAndSeedAsConstant)
{
    const FractalSpec julia{.julia = true, .seed = {0.0, 0.0}};
    // c = 0: z -> z^2, so |z0| < 1 stays bounded and |z0| > 1 escapes.
    EXPECT_TRUE(mandelbrotter::iteratePixel(julia, {0.5, 0.5}, kMaxIter).interior);
    EXPECT_FALSE(mandelbrotter::iteratePixel(julia, {1.2, 0.0}, kMaxIter).interior);
    // Explicit start/constant agree with the pixel mapping.
    const Complex p{0.3, -0.2};
    EXPECT_EQ(mandelbrotter::iteratePixel(julia, p, kMaxIter),
              mandelbrotter::iterate(julia, p, {0.0, 0.0}, kMaxIter));
    EXPECT_EQ(mandelbrotter::iteratePixel(kMandelbrot, p, kMaxIter),
              mandelbrotter::iterate(kMandelbrot, {0.0, 0.0}, p, kMaxIter));
    const auto start = mandelbrotter::orbitStart(julia, p);
    EXPECT_EQ(start.z0, p);
    EXPECT_EQ(start.c, julia.seed);
}

TEST(Kernel, OrbitStartsAtZ0AndStopsAtEscape)
{
    const std::vector<Complex> points = mandelbrotter::orbit(kMandelbrot, {1.0, 0.0}, 100);
    ASSERT_EQ(points.size(), 6U);  // 0, 1, 2, 5, 26, 677
    EXPECT_EQ(points[0], (Complex{0.0, 0.0}));
    EXPECT_EQ(points[1], (Complex{1.0, 0.0}));
    EXPECT_EQ(points[5], (Complex{677.0, 0.0}));
    EXPECT_GT(points.back().normSquared(), mandelbrotter::kBailoutRadiusSquared);
}

TEST(Kernel, OrbitIsCappedForInteriorPoints)
{
    const std::vector<Complex> points = mandelbrotter::orbit(kMandelbrot, {-1.0, 0.0}, 7);
    ASSERT_EQ(points.size(), 7U);  // period-2 orbit: 0, -1, 0, -1, ...
    EXPECT_EQ(points[2], (Complex{0.0, 0.0}));
    EXPECT_EQ(points[3], (Complex{-1.0, 0.0}));
    EXPECT_TRUE(mandelbrotter::orbit(kMandelbrot, {0.0, 0.0}, 0).empty());
}

TEST(Kernel, OrbitInJuliaModeStartsAtThePixel)
{
    const FractalSpec          julia{.julia = true, .seed = {-0.8, 0.156}};
    const std::vector<Complex> points = mandelbrotter::orbit(julia, {0.1, 0.2}, 3);
    ASSERT_EQ(points.size(), 3U);
    EXPECT_EQ(points[0], (Complex{0.1, 0.2}));
    const Complex expected = points[0] * points[0] + julia.seed;
    EXPECT_DOUBLE_EQ(points[1].re, expected.re);
    EXPECT_DOUBLE_EQ(points[1].im, expected.im);
}

}  // namespace
