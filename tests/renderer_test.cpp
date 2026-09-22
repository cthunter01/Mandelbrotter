#include "Mandelbrotter/renderer.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stop_token>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"
#include "Mandelbrotter/kernel.h"
#include "Mandelbrotter/render_settings.h"
#include "Mandelbrotter/viewport.h"

namespace
{

using mandelbrotter::IterationBuffer;
using mandelbrotter::PixelRect;
using mandelbrotter::PixelSize;
using mandelbrotter::ProgressiveRenderer;
using mandelbrotter::RenderCompletion;
using mandelbrotter::RenderJob;
using mandelbrotter::RenderSettings;
using mandelbrotter::TileResult;

constexpr PixelSize kSize{70, 50};  // not a multiple of the tile size on purpose

RenderSettings smallScene()
{
    RenderSettings s;
    s.view           = {{-0.75, 0.1}, 3.0};
    s.maxIterations  = 120;
    s.autoIterations = false;
    return s;
}

/// Runs a job to completion, collecting every tile in arrival order.
struct Collected
{
    std::vector<TileResult> tiles;
    RenderCompletion        completion;
    int                     completions{0};
};

Collected runJob(ProgressiveRenderer& renderer, RenderJob job,
                 std::chrono::milliseconds timeout = std::chrono::seconds(30))
{
    auto                    collected = std::make_shared<Collected>();
    auto                    mutex     = std::make_shared<std::mutex>();
    auto                    done      = std::make_shared<std::promise<void>>();
    const std::future<void> finished  = done->get_future();
    renderer.start(
        std::move(job),
        [collected, mutex](const TileResult& tile) {
            const std::scoped_lock lock(*mutex);
            collected->tiles.push_back(tile);
        },
        [collected, mutex, done](const RenderCompletion& completion) {
            const std::scoped_lock lock(*mutex);
            collected->completion = completion;
            if (++collected->completions == 1)
            {
                done->set_value();
            }
        });
    EXPECT_EQ(finished.wait_for(timeout), std::future_status::ready);
    const std::scoped_lock lock(*mutex);
    return *collected;
}

IterationBuffer applyAll(const std::vector<TileResult>& tiles, PixelSize size)
{
    IterationBuffer buffer(size.width, size.height);
    for (const TileResult& tile : tiles)
    {
        tile.copyInto(buffer);
    }
    return buffer;
}

TEST(Renderer, SyncRenderIsIndependentOfThreadCount)
{
    const auto one  = mandelbrotter::renderSync(smallScene(), kSize, 1);
    const auto four = mandelbrotter::renderSync(smallScene(), kSize, 4);
    ASSERT_TRUE(one.has_value());
    ASSERT_TRUE(four.has_value());
    EXPECT_EQ(*one, *four);
    EXPECT_EQ(one->size(), kSize);
    // Sanity: the scene contains both interior and exterior points.
    EXPECT_NE(std::ranges::count(one->interior, 1), 0);
    EXPECT_NE(std::ranges::count(one->interior, 0), 0);
}

TEST(Renderer, SyncRenderMatchesTheKernelPixelByPixel)
{
    const RenderSettings settings = smallScene();
    const auto           buffer   = mandelbrotter::renderSync(settings, kSize, 2);
    ASSERT_TRUE(buffer.has_value());
    const mandelbrotter::Viewport vp(settings.view, kSize);
    for (const auto& [x, y] :
         {std::pair{0, 0}, std::pair{69, 49}, std::pair{35, 25}, std::pair{64, 3}})
    {
        const auto expected = mandelbrotter::iteratePixel(
            settings.fractal, vp.pixelCenter({x, y}), mandelbrotter::effectiveIterations(settings));
        EXPECT_EQ(buffer->at(x, y).interior, expected.interior);
        EXPECT_NEAR(buffer->at(x, y).smoothIter, expected.smoothIter, 1e-4);
    }
}

TEST(Renderer, SyncRegionMatchesTheFullRender)
{
    const auto      full = mandelbrotter::renderSync(smallScene(), kSize, 2);
    const PixelRect band{0, 20, kSize.width, 15};
    const auto      part = mandelbrotter::renderSync(smallScene(), kSize, 2, band);
    ASSERT_TRUE(full.has_value());
    ASSERT_TRUE(part.has_value());
    EXPECT_EQ(part->size(), (PixelSize{kSize.width, 15}));
    for (int y = 0; y < 15; ++y)
    {
        for (int x = 0; x < kSize.width; ++x)
        {
            EXPECT_EQ(part->at(x, y), full->at(x, y + 20));
        }
    }
    const auto clipped =
        mandelbrotter::renderSync(smallScene(), kSize, 1, PixelRect{60, 45, 100, 100});
    ASSERT_TRUE(clipped.has_value());
    EXPECT_EQ(clipped->size(), (PixelSize{10, 5}));
    EXPECT_EQ(clipped->at(9, 4), full->at(69, 49));
    const auto empty = mandelbrotter::renderSync(smallScene(), kSize, 1, PixelRect{100, 100, 5, 5});
    ASSERT_TRUE(empty.has_value());
    EXPECT_TRUE(empty->size().empty());
}

TEST(Renderer, SyncRenderReportsProgressAndHonoursStop)
{
    int        last  = 0;
    int        total = 0;
    const auto buffer =
        mandelbrotter::renderSync(smallScene(), kSize, 3, std::nullopt, {}, [&](int done, int all) {
            EXPECT_EQ(done, last + 1);
            last  = done;
            total = all;
        });
    ASSERT_TRUE(buffer.has_value());
    EXPECT_EQ(total, 2);  // 70x50 in 64x64 tiles
    EXPECT_EQ(last, total);

    // Not const: the standard's request_stop() is non-const (libc++, MSVC); libstdc++'s const one
    // is an extension, which is why clang-tidy on Linux would otherwise ask for const here.
    std::stop_source source;  // NOLINT(misc-const-correctness)
    source.request_stop();
    EXPECT_FALSE(mandelbrotter::renderSync(smallScene(), kSize, 2, std::nullopt, source.get_token())
                     .has_value());
}

TEST(Renderer, TileGridCoversAreaOnceCentreFirst)
{
    const std::vector<PixelRect> tiles = mandelbrotter::tileGrid({0, 0, 70, 50}, 32);
    ASSERT_EQ(tiles.size(), 6U);
    std::vector<int> covered(static_cast<std::size_t>(70) * 50, 0);
    for (const PixelRect& t : tiles)
    {
        EXPECT_GE(t.x, 0);
        EXPECT_GE(t.y, 0);
        EXPECT_LE(t.right(), 70);
        EXPECT_LE(t.bottom(), 50);
        for (int y = t.y; y < t.bottom(); ++y)
        {
            for (int x = t.x; x < t.right(); ++x)
            {
                ++covered[(static_cast<std::size_t>(y) * 70) + static_cast<std::size_t>(x)];
            }
        }
    }
    EXPECT_TRUE(std::ranges::all_of(covered, [](int n) { return n == 1; }));
    // The first tile is the one nearest the centre (35, 25): the tile at (32, 0) or (32, 32) are
    // equidistant candidates, and stable sorting keeps grid order, so (32, 0) comes first.
    EXPECT_EQ(tiles.front(), (PixelRect{32, 0, 32, 32}));
    EXPECT_TRUE(mandelbrotter::tileGrid({0, 0, 0, 10}, 32).empty());
}

TEST(Renderer, ProgressiveFinalResultEqualsSyncRender)
{
    ProgressiveRenderer renderer;
    const Collected     result =
        runJob(renderer, RenderJob{.settings = smallScene(), .size = kSize, .threads = 3});
    EXPECT_FALSE(result.completion.cancelled);
    EXPECT_EQ(result.completions, 1);
    EXPECT_EQ(result.completion.generation, renderer.generation());
    const auto expected = mandelbrotter::renderSync(smallScene(), kSize, 1);
    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(applyAll(result.tiles, kSize), *expected);
    EXPECT_FALSE(renderer.busy());
}

TEST(Renderer, EveryPassCoversEveryTileInOrder)
{
    ProgressiveRenderer renderer;
    const Collected     result = runJob(
        renderer, RenderJob{.settings = smallScene(), .size = kSize, .tileSize = 32, .threads = 2});
    const std::vector<PixelRect> grid =
        mandelbrotter::tileGrid({0, 0, kSize.width, kSize.height}, 32);
    std::map<std::pair<int, int>, std::vector<int>> passesByTile;
    for (const TileResult& tile : result.tiles)
    {
        EXPECT_EQ(tile.generation, result.completion.generation);
        EXPECT_EQ(tile.smoothIter.size(), static_cast<std::size_t>(tile.rect.width) *
                                              static_cast<std::size_t>(tile.rect.height));
        EXPECT_EQ(tile.interior.size(), tile.smoothIter.size());
        passesByTile[{tile.rect.x, tile.rect.y}].push_back(tile.pass);
    }
    EXPECT_EQ(passesByTile.size(), grid.size());
    for (const auto& [origin, passes] : passesByTile)
    {
        EXPECT_EQ(passes, (std::vector<int>{8, 4, 2, 1})) << origin.first << "," << origin.second;
    }
}

TEST(Renderer, CoarsePassesAreBlockFilledPreviews)
{
    ProgressiveRenderer renderer;
    const Collected     result =
        runJob(renderer,
               RenderJob{.settings = smallScene(), .size = kSize, .passes = {8, 1}, .threads = 2});
    const auto expected = mandelbrotter::renderSync(smallScene(), kSize, 1);
    ASSERT_TRUE(expected.has_value());
    for (const TileResult& tile : result.tiles)
    {
        if (tile.pass != 8)
        {
            continue;
        }
        // Every 8x8 block carries the value of its top-left sample, which equals the exact pixel
        // there.
        for (int ly = 0; ly < tile.rect.height; ++ly)
        {
            for (int lx = 0; lx < tile.rect.width; ++lx)
            {
                const std::size_t i =
                    static_cast<std::size_t>(ly) * static_cast<std::size_t>(tile.rect.width) +
                    static_cast<std::size_t>(lx);
                const std::size_t anchor = static_cast<std::size_t>(ly - ly % 8) *
                                               static_cast<std::size_t>(tile.rect.width) +
                                           static_cast<std::size_t>(lx - lx % 8);
                EXPECT_EQ(tile.smoothIter[i], tile.smoothIter[anchor]);
                EXPECT_EQ(tile.interior[i], tile.interior[anchor]);
            }
        }
        const auto exact = expected->at(tile.rect.x, tile.rect.y);
        EXPECT_FLOAT_EQ(tile.smoothIter[0], static_cast<float>(exact.smoothIter));
    }
}

TEST(Renderer, CancelStopsAJobPromptlyAndReportsIt)
{
    ProgressiveRenderer renderer;
    RenderSettings      heavy = smallScene();
    heavy.maxIterations       = mandelbrotter::kMaxIterations;
    heavy.view = {{-0.5, 0.0},
                  0.5};  // mostly interior points: every pixel runs the full iteration count
    auto                          done     = std::make_shared<std::promise<RenderCompletion>>();
    std::future<RenderCompletion> finished = done->get_future();
    const std::uint64_t           generation =
        renderer.start(RenderJob{.settings = heavy, .size = {1200, 1200}, .threads = 2}, {},
                       [done](const RenderCompletion& c) { done->set_value(c); });
    EXPECT_TRUE(renderer.busy());
    const auto before = std::chrono::steady_clock::now();
    renderer.cancel();
    EXPECT_LT(std::chrono::steady_clock::now() - before, std::chrono::seconds(10));
    ASSERT_EQ(finished.wait_for(std::chrono::seconds(0)), std::future_status::ready);
    const RenderCompletion completion = finished.get();
    EXPECT_TRUE(completion.cancelled);
    EXPECT_EQ(completion.generation, generation);
    EXPECT_FALSE(renderer.busy());
}

TEST(Renderer, StartingAgainCancelsThePreviousJob)
{
    ProgressiveRenderer renderer;
    RenderSettings      heavy = smallScene();
    heavy.maxIterations       = mandelbrotter::kMaxIterations;
    heavy.view                = {{-0.5, 0.0}, 0.5};
    std::mutex                    mutex;
    std::vector<RenderCompletion> completions;
    const auto                    record = [&](const RenderCompletion& c) {
        const std::scoped_lock lock(mutex);
        completions.push_back(c);
    };
    const std::uint64_t first = renderer.start(
        RenderJob{.settings = heavy, .size = {1200, 1200}, .threads = 2}, {}, record);
    const Collected second =
        runJob(renderer, RenderJob{.settings = smallScene(), .size = kSize, .threads = 2});
    EXPECT_NE(first, second.completion.generation);
    EXPECT_FALSE(second.completion.cancelled);
    const std::scoped_lock lock(mutex);
    ASSERT_EQ(completions.size(), 1U);
    EXPECT_EQ(completions.front().generation, first);
    EXPECT_TRUE(completions.front().cancelled);
}

TEST(Renderer, DestructorCancelsARunningJob)
{
    auto                          done     = std::make_shared<std::promise<RenderCompletion>>();
    std::future<RenderCompletion> finished = done->get_future();
    {
        ProgressiveRenderer renderer;
        RenderSettings      heavy = smallScene();
        heavy.maxIterations       = mandelbrotter::kMaxIterations;
        heavy.view                = {{-0.5, 0.0}, 0.5};
        renderer.start(RenderJob{.settings = heavy, .size = {1200, 1200}, .threads = 2}, {},
                       [done](const RenderCompletion& c) { done->set_value(c); });
    }
    ASSERT_EQ(finished.wait_for(std::chrono::seconds(0)), std::future_status::ready);
    EXPECT_TRUE(finished.get().cancelled);
}

TEST(Renderer, OddPassListsAreSanitised)
{
    ProgressiveRenderer renderer;
    const Collected     result = runJob(
        renderer,
        RenderJob{.settings = smallScene(), .size = kSize, .passes = {0, 4, 4, -2}, .threads = 1});
    std::set<int> passes;
    for (const TileResult& tile : result.tiles)
    {
        passes.insert(tile.pass);
    }
    EXPECT_EQ(passes, (std::set<int>{4, 1}));
    const auto expected = mandelbrotter::renderSync(smallScene(), kSize, 1);
    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(applyAll(result.tiles, kSize), *expected);
}

TEST(Renderer, WorkerCountDefaultsToHardware)
{
    EXPECT_EQ(mandelbrotter::workerCount(3), 3U);
    EXPECT_GE(mandelbrotter::workerCount(0), 1U);
}

TEST(Renderer, TileCopyIsClipped)
{
    TileResult tile{.generation = 1,
                    .pass       = 1,
                    .rect       = {-1, -1, 3, 3},
                    .smoothIter = std::vector<float>(9, 0.0F),
                    .interior   = std::vector<std::uint8_t>(9, 0)};
    tile.smoothIter[4] = 5.0F;  // local (1, 1) -> buffer (0, 0)
    tile.interior[8]   = 1;     // local (2, 2) -> buffer (1, 1)
    IterationBuffer buffer(2, 2);
    tile.copyInto(buffer);
    EXPECT_FLOAT_EQ(buffer.smoothIter[0], 5.0F);
    EXPECT_EQ(buffer.at(1, 1).interior, true);
    EXPECT_EQ(buffer.at(1, 0).interior, false);
}

}  // namespace
