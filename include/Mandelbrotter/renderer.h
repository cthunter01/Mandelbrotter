#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <stop_token>
#include <thread>
#include <vector>

#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"
#include "Mandelbrotter/render_settings.h"

namespace mandelbrotter
{

inline constexpr int kDefaultTileSize = 64;

/// Progress callback: (tiles done, tiles total). May be called from worker threads, but never
/// concurrently.
using ProgressCallback = std::function<void(int done, int total)>;

/// One progressive render: coarse passes first (pixel step 8, 4, 2), then every pixel.
struct RenderJob
{
    RenderSettings   settings;
    PixelSize        size;
    std::vector<int> passes{8, 4, 2, 1};  ///< pixel steps, decreasing; the last should be 1
    int              tileSize{kDefaultTileSize};
    unsigned         threads{0};  ///< 0 = hardware concurrency
};

/// The state of one tile after one pass. Every pixel of `rect` has a value: samples computed at
/// this or an earlier pass, and block-filled copies of the nearest sample for the rest.
struct TileResult
{
    std::uint64_t             generation{};
    int                       pass{};  ///< the pixel step of the pass that produced this
    PixelRect                 rect;
    std::vector<float>        smoothIter;  ///< rect.width * rect.height, row-major
    std::vector<std::uint8_t> interior;    ///< same layout

    /// Copies the tile into `buffer` (clipped to it).
    void copyInto(IterationBuffer& buffer) const noexcept;
};

struct RenderCompletion
{
    std::uint64_t             generation{};
    bool                      cancelled{};
    std::chrono::milliseconds elapsed{};
};

using TileCallback       = std::function<void(const TileResult&)>;
using CompletionCallback = std::function<void(const RenderCompletion&)>;

/// Runs one RenderJob at a time on a pool of worker threads. Starting a new job cancels the current
/// one.
///
/// Callbacks run on worker threads (onTile possibly on several at once; onDone exactly once per
/// job, after all tile callbacks for that job have returned). They must not throw and must not call
/// back into the renderer.
class ProgressiveRenderer
{
public:
    ProgressiveRenderer() = default;
    ~ProgressiveRenderer();
    ProgressiveRenderer(const ProgressiveRenderer&)            = delete;
    ProgressiveRenderer& operator=(const ProgressiveRenderer&) = delete;
    ProgressiveRenderer(ProgressiveRenderer&&)                 = delete;
    ProgressiveRenderer& operator=(ProgressiveRenderer&&)      = delete;

    /// Cancels any running job (waiting for it to finish) and starts `job`. Returns the job's
    /// generation id.
    std::uint64_t start(RenderJob job, TileCallback onTile, CompletionCallback onDone);
    /// Requests cancellation and waits until the job has reported completion.
    void cancel();

    [[nodiscard]] bool busy() const noexcept { return m_busy.load(); }
    /// The id of the most recently started job (0 before the first start()).
    [[nodiscard]] std::uint64_t generation() const noexcept { return m_generation; }

private:
    std::uint64_t     m_generation{0};
    std::atomic<bool> m_busy{false};
    std::jthread      m_coordinator;
};

/// Blocking full-resolution render. With `region`, only those pixels of the `size` image are
/// computed and the result has the region's size. Returns nullopt if `stop` was requested before
/// completion.
[[nodiscard]] std::optional<IterationBuffer> renderSync(
    const RenderSettings& settings, PixelSize size, unsigned threads = 0,
    std::optional<PixelRect> region = std::nullopt, const std::stop_token& stop = {},
    const ProgressCallback& progress = {});

/// Splits `area` into tiles of at most tileSize x tileSize, ordered from the centre outwards.
[[nodiscard]] std::vector<PixelRect> tileGrid(PixelRect area, int tileSize);

/// The number of worker threads a job with `requested` (0 = auto) will use.
[[nodiscard]] unsigned workerCount(unsigned requested) noexcept;

}  // namespace mandelbrotter
