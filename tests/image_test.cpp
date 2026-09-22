#include "Mandelbrotter/image.h"

#include <cstdint>

#include <gtest/gtest.h>

#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/kernel.h"

namespace
{

using mandelbrotter::IterationBuffer;
using mandelbrotter::IterationResult;
using mandelbrotter::PixelRect;
using mandelbrotter::Rgb;
using mandelbrotter::RgbImage;

TEST(Image, BuffersAreZeroInitialised)
{
    const IterationBuffer buffer(4, 3);
    EXPECT_EQ(buffer.smoothIter.size(), 12U);
    EXPECT_EQ(buffer.interior.size(), 12U);
    EXPECT_EQ(buffer.at(3, 2), (IterationResult{0.0, false}));
    const RgbImage image(4, 3, {1, 2, 3});
    EXPECT_EQ(image.pixels.size(), 36U);
    EXPECT_EQ(image.at(3, 2), (Rgb{1, 2, 3}));
    EXPECT_TRUE(RgbImage(-1, 5).pixels.empty());
}

TEST(Image, SetAndGetRoundTrip)
{
    IterationBuffer buffer(3, 3);
    buffer.set(1, 2, {.smoothIter = 12.5, .interior = false});
    buffer.set(2, 0, {.smoothIter = 7.0, .interior = true});
    EXPECT_EQ(buffer.at(1, 2), (IterationResult{12.5, false}));
    EXPECT_EQ(buffer.at(2, 0), (IterationResult{7.0, true}));
    RgbImage image(3, 3);
    image.set(2, 1, {10, 20, 30});
    EXPECT_EQ(image.at(2, 1), (Rgb{10, 20, 30}));
    EXPECT_EQ(image.at(1, 2), mandelbrotter::kBlack);
}

TEST(Image, IntersectClips)
{
    EXPECT_EQ(mandelbrotter::intersect({0, 0, 10, 10}, {5, 5, 10, 10}), (PixelRect{5, 5, 5, 5}));
    EXPECT_EQ(mandelbrotter::intersect({-3, -3, 5, 5}, {0, 0, 10, 10}), (PixelRect{0, 0, 2, 2}));
    EXPECT_TRUE(mandelbrotter::intersect({0, 0, 10, 10}, {10, 0, 5, 5}).empty());
    EXPECT_TRUE(mandelbrotter::intersect({0, 0, 10, 10}, {2, 2, 0, 5}).empty());
}

TEST(Image, ColorizeRectOnlyTouchesTheRect)
{
    IterationBuffer buffer(4, 4);
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            buffer.set(x, y, {.smoothIter = 0.0, .interior = false});
        }
    }
    const auto& palette = mandelbrotter::paletteOrDefault("rainbow");  // t = 0 is pure red
    RgbImage    out(4, 4, {7, 7, 7});
    mandelbrotter::colorize(buffer, {1, 1, 2, 10}, palette, {}, out);  // rect overhangs: clipped
    EXPECT_EQ(out.at(0, 0), (Rgb{7, 7, 7}));
    EXPECT_EQ(out.at(1, 1), (Rgb{255, 0, 0}));
    EXPECT_EQ(out.at(2, 3), (Rgb{255, 0, 0}));
    EXPECT_EQ(out.at(3, 3), (Rgb{7, 7, 7}));
    EXPECT_EQ(out.at(0, 3), (Rgb{7, 7, 7}));
}

TEST(Image, ColorizeResizesMismatchedOutputAndPaintsInteriorBlack)
{
    IterationBuffer buffer(2, 1);
    buffer.set(0, 0, {.smoothIter = 0.0, .interior = false});
    buffer.set(1, 0, {.smoothIter = 5.0, .interior = true});
    RgbImage out;  // empty
    mandelbrotter::colorize(buffer, mandelbrotter::paletteOrDefault("rainbow"), {}, out);
    EXPECT_EQ(out.size(), buffer.size());
    EXPECT_EQ(out.at(0, 0), (Rgb{255, 0, 0}));
    EXPECT_EQ(out.at(1, 0), mandelbrotter::kBlack);
    EXPECT_EQ(mandelbrotter::colorized(buffer, mandelbrotter::paletteOrDefault("rainbow"), {}),
              out);
}

TEST(Image, DownsampleAveragesBlocks)
{
    RgbImage image(4, 2);
    image.set(0, 0, {0, 0, 0});
    image.set(1, 0, {100, 0, 0});
    image.set(0, 1, {0, 200, 0});
    image.set(1, 1, {0, 0, 100});
    for (int x = 2; x < 4; ++x)
    {
        for (int y = 0; y < 2; ++y)
        {
            image.set(x, y, {255, 255, 255});
        }
    }
    const RgbImage small = mandelbrotter::downsample(image, 2);
    EXPECT_EQ(small.size(), (mandelbrotter::PixelSize{2, 1}));
    EXPECT_EQ(small.at(0, 0), (Rgb{25, 50, 25}));
    EXPECT_EQ(small.at(1, 0), (Rgb{255, 255, 255}));
    EXPECT_EQ(mandelbrotter::downsample(image, 1), image);
    EXPECT_EQ(mandelbrotter::downsample(image, 3).size(), (mandelbrotter::PixelSize{1, 0}));
}

TEST(Image, BlitShiftedClipsAndLeavesTheRestUntouched)
{
    RgbImage src(3, 2);
    for (int y = 0; y < 2; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            src.set(x, y, {static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y), 9});
        }
    }
    RgbImage dst(3, 2, {7, 7, 7});
    mandelbrotter::blitShifted(src, 1, 1, dst);
    EXPECT_EQ(dst.at(0, 0), (Rgb{7, 7, 7}));
    EXPECT_EQ(dst.at(1, 1), (Rgb{0, 0, 9}));
    EXPECT_EQ(dst.at(2, 1), (Rgb{1, 0, 9}));
    EXPECT_EQ(dst.at(2, 0), (Rgb{7, 7, 7}));

    RgbImage left(3, 2, {7, 7, 7});
    mandelbrotter::blitShifted(src, -2, 0, left);
    EXPECT_EQ(left.at(0, 0), (Rgb{2, 0, 9}));
    EXPECT_EQ(left.at(0, 1), (Rgb{2, 1, 9}));
    EXPECT_EQ(left.at(1, 0), (Rgb{7, 7, 7}));

    RgbImage far(3, 2, {7, 7, 7});
    mandelbrotter::blitShifted(src, 10, 0, far);
    EXPECT_EQ(far, (RgbImage{3, 2, {7, 7, 7}}));
}

}  // namespace
