#include "Mandelbrotter/RenderSettings.h"

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

namespace
{

using mandelbrotter::RenderSettings;

TEST(RenderSettings, AutoIterationsStartAtBaseAndGrowMonotonically)
{
    EXPECT_EQ(mandelbrotter::autoIterationsFor(0.5, 256), 256);
    EXPECT_EQ(mandelbrotter::autoIterationsFor(1.0, 256), 256);
    int previous = 256;
    for (int step = 0; step < 60; ++step)
    {
        const double zoom    = std::pow(1.7, step);
        const int    current = mandelbrotter::autoIterationsFor(zoom, 256);
        EXPECT_GE(current, previous) << zoom;
        EXPECT_LE(current, mandelbrotter::kMaxIterations);
        previous = current;
    }
    const double d = std::log2(1e6);
    EXPECT_EQ(mandelbrotter::autoIterationsFor(1e6, 256),
              static_cast<int>(256 + (64 * d) + (0.5 * d * d)));
    // 1e300 alone asks for about 560 000; a large base pushes it over the cap.
    EXPECT_GT(mandelbrotter::autoIterationsFor(1e300, 256), 500'000);
    EXPECT_LT(mandelbrotter::autoIterationsFor(1e300, 256), mandelbrotter::kMaxIterations);
    EXPECT_EQ(mandelbrotter::autoIterationsFor(1e300, 900'000), mandelbrotter::kMaxIterations);
    EXPECT_EQ(mandelbrotter::autoIterationsFor(std::numeric_limits<double>::infinity(), 256),
              mandelbrotter::kMaxIterations);
    EXPECT_EQ(mandelbrotter::autoIterationsFor(2.0, 0), mandelbrotter::kMinIterations + 64);
}

TEST(RenderSettings, EffectiveIterationsRespectsAutoFlag)
{
    RenderSettings s;
    s.view.zoom      = 1024.0;  // 10 doublings
    s.maxIterations  = 100;
    s.autoIterations = false;
    EXPECT_EQ(mandelbrotter::effectiveIterations(s), 100);
    s.autoIterations = true;
    EXPECT_EQ(mandelbrotter::effectiveIterations(s), 100 + (10 * 64) + 50);  // 0.5 * 10^2
    s.maxIterations = 10'000'000;
    EXPECT_EQ(mandelbrotter::effectiveIterations(s), mandelbrotter::kMaxIterations);
}

TEST(RenderSettings, ColoringChangesDoNotNeedRerender)
{
    const RenderSettings a;
    RenderSettings       b = a;
    b.coloring.palette     = "fire";
    b.coloring.density     = 12.0;
    b.coloring.offset      = 0.3;
    EXPECT_NE(a, b);
    EXPECT_FALSE(mandelbrotter::needsRerender(a, b));
    EXPECT_FALSE(mandelbrotter::needsRerender(a, a));
}

TEST(RenderSettings, GeometryOrIterationChangesNeedRerender)
{
    const RenderSettings a;
    RenderSettings       view = a;
    view.view.zoom *= 2.0;
    EXPECT_TRUE(mandelbrotter::needsRerender(a, view));
    RenderSettings fractal   = a;
    fractal.fractal.exponent = 3;
    EXPECT_TRUE(mandelbrotter::needsRerender(a, fractal));
    RenderSettings iterations = a;
    iterations.maxIterations += 1;
    EXPECT_TRUE(mandelbrotter::needsRerender(a, iterations));
    // Turning auto on at zoom 1 changes nothing effective.
    RenderSettings autoOn = a;
    autoOn.autoIterations = !a.autoIterations;
    EXPECT_FALSE(mandelbrotter::needsRerender(a, autoOn));
}

}  // namespace
