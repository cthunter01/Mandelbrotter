#include "Mandelbrotter/exporter.h"

#include <stdexcept>
#include <stop_token>

#include <gtest/gtest.h>

#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"
#include "Mandelbrotter/palette.h"
#include "Mandelbrotter/render_settings.h"
#include "Mandelbrotter/renderer.h"

namespace
{

using mandelbrotter::ExportOptions;
using mandelbrotter::PixelSize;
using mandelbrotter::RenderSettings;
using mandelbrotter::RgbImage;

RenderSettings scene()
{
    RenderSettings s;
    s.view             = {{-0.75, 0.1}, 3.0};
    s.maxIterations    = 80;
    s.autoIterations   = false;
    s.coloring.palette = "fire";
    return s;
}

TEST(Exporter, PlainExportEqualsColorizedSyncRender)
{
    const ExportOptions options{.size = {90, 70}, .supersample = 1};
    const auto          exported = mandelbrotter::renderForExport(scene(), options);
    ASSERT_TRUE(exported.has_value());
    const auto buffer = mandelbrotter::renderSync(scene(), options.size, 2);
    ASSERT_TRUE(buffer.has_value());
    const RgbImage expected = mandelbrotter::colorized(
        *buffer, mandelbrotter::paletteOrDefault("fire"), scene().coloring);
    EXPECT_EQ(*exported, expected);
}

TEST(Exporter, SupersamplingEqualsRenderingLargeAndDownsampling)
{
    const ExportOptions options{.size = {64, 48}, .supersample = 2};
    const auto          exported = mandelbrotter::renderForExport(scene(), options);
    ASSERT_TRUE(exported.has_value());
    EXPECT_EQ(exported->size(), options.size);
    const auto big = mandelbrotter::renderSync(scene(), {128, 96}, 2);
    ASSERT_TRUE(big.has_value());
    const RgbImage expected = mandelbrotter::downsample(
        mandelbrotter::colorized(*big, mandelbrotter::paletteOrDefault("fire"), scene().coloring),
        2);
    EXPECT_EQ(*exported, expected);
    // Anti-aliasing changes the picture compared with a plain render.
    const auto plain =
        mandelbrotter::renderForExport(scene(), {.size = {64, 48}, .supersample = 1});
    ASSERT_TRUE(plain.has_value());
    EXPECT_NE(*exported, *plain);
}

TEST(Exporter, TallImagesAreAssembledFromBandsSeamlessly)
{
    // 4x supersampling of 40x300 = 160x1200 supersampled rows: three 512-row bands.
    const ExportOptions options{.size = {40, 300}, .supersample = 4};
    const auto          exported = mandelbrotter::renderForExport(scene(), options);
    ASSERT_TRUE(exported.has_value());
    const auto big = mandelbrotter::renderSync(scene(), {160, 1200}, 4);
    ASSERT_TRUE(big.has_value());
    const RgbImage expected = mandelbrotter::downsample(
        mandelbrotter::colorized(*big, mandelbrotter::paletteOrDefault("fire"), scene().coloring),
        4);
    EXPECT_EQ(*exported, expected);
}

TEST(Exporter, ReportsProgressAndHonoursStop)
{
    int        last     = 0;
    int        total    = 0;
    const auto exported = mandelbrotter::renderForExport(
        scene(), {.size = {100, 100}, .supersample = 2}, {}, [&](int done, int all) {
            EXPECT_GT(done, last);
            last  = done;
            total = all;
        });
    ASSERT_TRUE(exported.has_value());
    EXPECT_EQ(total, 16);  // 200x200 in 64x64 tiles
    EXPECT_EQ(last, total);

    const std::stop_source source;
    source.request_stop();
    EXPECT_FALSE(mandelbrotter::renderForExport(scene(), {.size = {20, 20}}, source.get_token())
                     .has_value());
}

TEST(Exporter, RejectsBadSizes)
{
    EXPECT_THROW(static_cast<void>(mandelbrotter::renderForExport(scene(), {.size = {0, 10}})),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(mandelbrotter::renderForExport(
                     scene(), {.size = {10, mandelbrotter::kMaxExportDimension + 1}})),
                 std::invalid_argument);
    const auto clamped =
        mandelbrotter::renderForExport(scene(), {.size = {8, 8}, .supersample = 99});
    ASSERT_TRUE(clamped.has_value());
    EXPECT_EQ(clamped->size(), (PixelSize{8, 8}));
}

}  // namespace
