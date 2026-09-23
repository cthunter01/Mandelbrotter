#include "Mandelbrotter/ProgressiveRenderer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/ReferenceOrbit.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"
#include "Mandelbrotter/kernel.h"
#include "Mandelbrotter/perturbation.h"

namespace mandelbrotter
{

namespace
{

/// Everything constant for the duration of a job.
struct Scene
{
    FractalSpec                   fractal;
    Viewport                      viewport;
    int                           maxIterations;
    std::optional<ReferenceOrbit> reference;  ///< the centre's orbit, in perturbation mode

    /// Computes the reference orbit when the zoom calls for one; `stop` cuts that short.
    Scene(const RenderSettings& settings, PixelSize size, const std::stop_token& stop)
      : fractal(settings.fractal),
        viewport(settings.view, size),
        maxIterations(effectiveIterations(settings))
    {
        const double zoom = viewport.view().zoom;
        if (usesPerturbation(zoom))
        {
            // A centre that came in as doubles has fewer bits than its zoom needs; extending is
            // exact.
            const BigComplex& center = viewport.view().center;
            const int         bits   = std::max(center.fractionBits(), fractionBitsFor(zoom));
            reference.emplace(fractal, center.withFractionBits(bits), maxIterations, stop);
        }
    }

    [[nodiscard]] IterationResult sample(int x, int y) const noexcept
    {
        if (reference)
        {
            return iteratePerturbed(*reference, viewport.offsetFromCenter(x + 0.5, y + 0.5),
                                    maxIterations);
        }
        return iteratePixel(fractal, viewport.pixelCenter({x, y}), maxIterations);
    }
};

/// A tile's accumulated results across passes.
struct TileState
{
    PixelRect                 rect;
    std::vector<float>        smoothIter;
    std::vector<std::uint8_t> interior;

    explicit TileState(PixelRect r)
      : rect(r),
        smoothIter(static_cast<std::size_t>(r.width) * static_cast<std::size_t>(r.height), 0.0F),
        interior(smoothIter.size(), 0)
    {
    }

    [[nodiscard]] std::size_t index(int lx, int ly) const noexcept
    {
        return static_cast<std::size_t>(ly) * static_cast<std::size_t>(rect.width) +
               static_cast<std::size_t>(lx);
    }

    void fill(int lx, int ly, int step, IterationResult r) noexcept
    {
        const int          endX   = std::min(lx + step, rect.width);
        const int          endY   = std::min(ly + step, rect.height);
        const auto         value  = static_cast<float>(r.smoothIter);
        const std::uint8_t inside = r.interior ? 1 : 0;
        for (int y = ly; y < endY; ++y)
        {
            for (int x = lx; x < endX; ++x)
            {
                const std::size_t i = index(x, y);
                smoothIter[i]       = value;
                interior[i]         = inside;
            }
        }
    }
};

/// Computes one pass over a tile. Samples lie on the tile-local grid (lx % step == 0, ly % step ==
/// 0); those already computed by the previous (coarser) pass are reused. Returns false if cancelled
/// part-way.
bool renderPass(const Scene& scene, TileState& tile, int step, int previousStep,
                const std::stop_token& stop)
{
    int sinceCheck = 0;
    for (int ly = 0; ly < tile.rect.height; ly += step)
    {
        for (int lx = 0; lx < tile.rect.width; lx += step)
        {
            if (++sinceCheck == 16)
            {
                sinceCheck = 0;
                if (stop.stop_requested())
                {
                    return false;
                }
            }
            const bool reuse = previousStep > 0 && lx % previousStep == 0 && ly % previousStep == 0;
            IterationResult r{};
            if (reuse)
            {
                const std::size_t i = tile.index(lx, ly);
                r                   = {.smoothIter = static_cast<double>(tile.smoothIter[i]),
                                       .interior   = tile.interior[i] != 0};
            }
            else
            {
                r = scene.sample(tile.rect.x + lx, tile.rect.y + ly);
            }
            tile.fill(lx, ly, step, r);
        }
    }
    return !stop.stop_requested();
}

TileResult toResult(const TileState& tile, std::uint64_t generation, int pass)
{
    return {.generation = generation,
            .pass       = pass,
            .rect       = tile.rect,
            .smoothIter = tile.smoothIter,
            .interior   = tile.interior};
}

/// Runs `work(tileIndex)` over all tiles on `threads` workers until done or cancelled.
template <typename Work>
void forEachTile(std::size_t tileCount, unsigned threads, const std::stop_token& stop,
                 const Work& work)
{
    std::atomic<std::size_t>  next{0};
    std::vector<std::jthread> workers;
    workers.reserve(threads);
    for (unsigned i = 0; i < threads; ++i)
    {
        workers.emplace_back([&] {
            for (std::size_t t = next++; t < tileCount; t = next++)
            {
                if (stop.stop_requested())
                {
                    return;
                }
                work(t);
            }
        });
    }
    // jthreads join on destruction.
}

std::vector<int> sanitizedPasses(std::vector<int> passes)
{
    std::erase_if(passes, [](int step) { return step < 1; });
    std::ranges::sort(passes, std::ranges::greater{});
    passes.erase(std::ranges::unique(passes).begin(), passes.end());
    if (passes.empty() || passes.back() != 1)
    {
        passes.push_back(1);
    }
    return passes;
}

void coordinate(const std::stop_token& stop, const RenderJob& job, std::uint64_t generation,
                const TileCallback& onTile, const CompletionCallback& onDone)
{
    const auto                   startTime = std::chrono::steady_clock::now();
    const Scene                  scene(job.settings, job.size, stop);
    const std::vector<PixelRect> rects =
        tileGrid({0, 0, job.size.width, job.size.height}, job.tileSize);
    std::vector<TileState> tiles;
    tiles.reserve(rects.size());
    for (const PixelRect rect : rects)
    {
        tiles.emplace_back(rect);
    }

    bool cancelled    = stop.stop_requested();  // the reference orbit may have been cut short
    int  previousStep = 0;
    for (const int step : sanitizedPasses(job.passes))
    {
        if (cancelled)
        {
            break;
        }
        forEachTile(tiles.size(), workerCount(job.threads), stop, [&](std::size_t t) {
            if (renderPass(scene, tiles[t], step, previousStep, stop) && onTile)
            {
                onTile(toResult(tiles[t], generation, step));
            }
        });
        if (stop.stop_requested())
        {
            cancelled = true;
            break;
        }
        previousStep = step;
    }

    if (onDone)
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime);
        onDone({.generation = generation, .cancelled = cancelled, .elapsed = elapsed});
    }
}

}  // namespace

void TileResult::copyInto(IterationBuffer& buffer) const noexcept
{
    const PixelRect target = intersect(rect, buffer.bounds());
    if (target.empty())
    {
        return;
    }
    const auto count = static_cast<std::size_t>(target.width);
    for (int y = target.y; y < target.bottom(); ++y)
    {
        const std::size_t from =
            static_cast<std::size_t>(y - rect.y) * static_cast<std::size_t>(rect.width) +
            static_cast<std::size_t>(target.x - rect.x);
        const std::size_t to = buffer.index(target.x, y);
        std::memcpy(&buffer.smoothIter[to], &smoothIter[from], count * sizeof(float));
        std::memcpy(&buffer.interior[to], &interior[from], count);
    }
}

ProgressiveRenderer::~ProgressiveRenderer()
{
    cancel();
}

std::uint64_t ProgressiveRenderer::start(RenderJob job, TileCallback onTile,
                                         CompletionCallback onDone)
{
    cancel();
    const std::uint64_t generation = ++m_generation;
    m_busy.store(true);
    m_coordinator =
        std::jthread([this, job = std::move(job), onTile = std::move(onTile),
                      onDone = std::move(onDone), generation](const std::stop_token& stop) {
            coordinate(stop, job, generation, onTile, onDone);
            m_busy.store(false);
        });
    return generation;
}

void ProgressiveRenderer::cancel()
{
    if (m_coordinator.joinable())
    {
        m_coordinator.request_stop();
        m_coordinator.join();
    }
}

std::optional<IterationBuffer> renderSync(const RenderSettings& settings, PixelSize size,
                                          unsigned threads, std::optional<PixelRect> region,
                                          const std::stop_token&  stop,
                                          const ProgressCallback& progress)
{
    const PixelRect area = intersect(region.value_or(PixelRect{0, 0, size.width, size.height}),
                                     {0, 0, size.width, size.height});
    IterationBuffer buffer(area.width, area.height);
    if (area.empty())
    {
        return buffer;
    }
    const Scene scene(settings, size, stop);
    if (stop.stop_requested())
    {
        return std::nullopt;
    }
    const std::vector<PixelRect> rects = tileGrid(area, kDefaultTileSize);
    std::mutex                   progressMutex;
    int                          done = 0;
    forEachTile(rects.size(), workerCount(threads), stop, [&](std::size_t t) {
        const PixelRect rect = rects[t];
        for (int y = rect.y; y < rect.bottom(); ++y)
        {
            if (stop.stop_requested())
            {
                return;
            }
            for (int x = rect.x; x < rect.right(); ++x)
            {
                buffer.set(x - area.x, y - area.y, scene.sample(x, y));
            }
        }
        if (progress)
        {
            const std::scoped_lock lock(progressMutex);
            progress(++done, static_cast<int>(rects.size()));
        }
    });
    if (stop.stop_requested())
    {
        return std::nullopt;
    }
    return buffer;
}

std::vector<PixelRect> tileGrid(PixelRect area, int tileSize)
{
    std::vector<PixelRect> tiles;
    if (area.empty())
    {
        return tiles;
    }
    tileSize = std::max(tileSize, 1);
    for (int y = area.y; y < area.bottom(); y += tileSize)
    {
        for (int x = area.x; x < area.right(); x += tileSize)
        {
            tiles.push_back({x, y, std::min(tileSize, area.right() - x),
                             std::min(tileSize, area.bottom() - y)});
        }
    }
    const double centerX  = area.x + area.width / 2.0;
    const double centerY  = area.y + area.height / 2.0;
    const auto   distance = [&](const PixelRect& t) {
        const double dx = t.x + t.width / 2.0 - centerX;
        const double dy = t.y + t.height / 2.0 - centerY;
        return dx * dx + dy * dy;
    };
    std::ranges::stable_sort(tiles, {}, distance);
    return tiles;
}

bool usesPerturbation(double zoom) noexcept
{
    return zoom > kPerturbationZoom;
}

unsigned workerCount(unsigned requested) noexcept
{
    if (requested > 0)
    {
        return requested;
    }
    const unsigned hardware = std::thread::hardware_concurrency();
    return hardware > 0 ? hardware : 1;
}

}  // namespace mandelbrotter
