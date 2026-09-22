#include "Mandelbrotter/palette.h"

#include <cstdlib>
#include <functional>
#include <limits>
#include <set>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace
{

using mandelbrotter::ColoringSettings;
using mandelbrotter::Palette;
using mandelbrotter::Rgb;

int channelDistance(Rgb a, Rgb b)
{
    return std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b);
}

TEST(Palette, SamplesHitTheStops)
{
    const Palette p("test", {{0.0, {255, 0, 0}}, {0.5, {0, 255, 0}}});
    EXPECT_EQ(p.sample(0.0), (Rgb{255, 0, 0}));
    EXPECT_LE(channelDistance(p.sample(0.5), {0, 255, 0}), 2);
}

TEST(Palette, InterpolatesLinearlyAndWrapsBackToTheFirstStop)
{
    const Palette p("test", {{0.0, {0, 0, 0}}, {0.5, {200, 100, 0}}});
    EXPECT_LE(channelDistance(p.sample(0.25), {100, 50, 0}), 3);
    // From t = 0.5 the gradient runs back to the first stop at t = 1.
    EXPECT_LE(channelDistance(p.sample(0.75), {100, 50, 0}), 3);
    EXPECT_LE(channelDistance(p.sample(0.999), {0, 0, 0}), 3);
}

TEST(Palette, WrapsPeriodicallyAndSurvivesBadInput)
{
    const Palette& p = mandelbrotter::paletteOrDefault("classic");
    EXPECT_EQ(p.sample(1.25), p.sample(0.25));
    EXPECT_EQ(p.sample(-0.75), p.sample(0.25));
    EXPECT_EQ(p.sample(1.0), p.sample(0.0));
    EXPECT_EQ(p.sample(std::numeric_limits<double>::infinity()), p.sample(0.0));
    EXPECT_EQ(p.sample(std::numeric_limits<double>::quiet_NaN()), p.sample(0.0));
}

TEST(Palette, StopsAreSortedAndFirstIsAnchoredAtZero)
{
    const Palette p("test", {{0.6, {1, 1, 1}}, {0.2, {9, 9, 9}}});
    ASSERT_EQ(p.stops().size(), 2U);
    EXPECT_DOUBLE_EQ(p.stops()[0].position, 0.0);
    EXPECT_EQ(p.stops()[0].color, (Rgb{9, 9, 9}));
    EXPECT_DOUBLE_EQ(p.stops()[1].position, 0.6);
    const Palette empty("empty", {});
    EXPECT_EQ(empty.sample(0.3), mandelbrotter::kBlack);
}

TEST(Palette, InteriorIsBlack)
{
    const Palette& p = mandelbrotter::paletteOrDefault("rainbow");
    EXPECT_EQ(mandelbrotter::colorFor({.smoothIter = 100.0, .interior = true}, p, {}),
              mandelbrotter::kBlack);
}

TEST(Palette, DensityAndOffsetMapIterationsOntoTheCycle)
{
    const Palette&         p = mandelbrotter::paletteOrDefault("rainbow");
    const ColoringSettings s{.palette = "rainbow", .density = 64.0, .offset = 0.0};
    EXPECT_EQ(mandelbrotter::colorFor({.smoothIter = 0.0}, p, s), p.sample(0.0));
    EXPECT_EQ(mandelbrotter::colorFor({.smoothIter = 16.0}, p, s), p.sample(0.25));
    EXPECT_EQ(mandelbrotter::colorFor({.smoothIter = 64.0}, p, s), p.sample(0.0));
    const ColoringSettings shifted{.palette = "rainbow", .density = 64.0, .offset = 0.5};
    EXPECT_EQ(mandelbrotter::colorFor({.smoothIter = 16.0}, p, shifted), p.sample(0.75));
    const ColoringSettings tiny{
        .palette = "rainbow", .density = 0.0, .offset = 0.0};  // clamped to 1
    EXPECT_EQ(mandelbrotter::colorFor({.smoothIter = 0.25}, p, tiny), p.sample(0.25));
}

TEST(Palette, BuiltinsAreNamedUniquelyAndResolvable)
{
    const auto names = mandelbrotter::paletteNames();
    EXPECT_GE(names.size(), 6U);
    std::set<std::string, std::less<>> unique;
    for (const auto name : names)
    {
        unique.emplace(name);
        const Palette* p = mandelbrotter::findPalette(name);
        ASSERT_NE(p, nullptr) << name;
        EXPECT_EQ(p->name(), name);
    }
    EXPECT_EQ(unique.size(), names.size());
    for (const std::string_view expected :
         {"classic", "grayscale", "fire", "ocean", "rainbow", "electric"})
    {
        EXPECT_TRUE(unique.contains(expected)) << expected;
    }
    EXPECT_EQ(mandelbrotter::findPalette("no-such-palette"), nullptr);
    EXPECT_EQ(&mandelbrotter::paletteOrDefault("no-such-palette"),
              &mandelbrotter::builtinPalettes().front());
}

TEST(Palette, GrayscaleMidpointIsMidGray)
{
    const Palette& p = mandelbrotter::paletteOrDefault("grayscale");
    EXPECT_LE(channelDistance(p.sample(0.25), {128, 128, 128}), 6);
    EXPECT_LE(channelDistance(p.sample(0.5), {255, 255, 255}), 3);
}

}  // namespace
