#pragma once

#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/palette.h"
#include "Mandelbrotter/viewport.h"

namespace mandelbrotter
{

inline constexpr int kMinIterations     = 1;
inline constexpr int kMaxIterations     = 100000;
inline constexpr int kDefaultIterations = 256;
/// Extra iterations added per doubling of the zoom when auto-iterations is on.
inline constexpr int kIterationsPerZoomDoubling = 64;

/// Everything that determines the picture. Saved in bookmarks and set from the command line.
struct RenderSettings
{
    FractalSpec      fractal{};
    ViewSpec         view{};
    int              maxIterations{kDefaultIterations};
    bool             autoIterations{true};
    ColoringSettings coloring{};

    bool operator==(const RenderSettings&) const = default;
};

/// The iteration limit for a zoom level: `base` at zoom <= 1, growing logarithmically, capped.
[[nodiscard]] int autoIterationsFor(double zoom, int base) noexcept;

/// The iteration limit actually used: maxIterations, or autoIterationsFor(zoom, maxIterations) in
/// auto mode.
[[nodiscard]] int effectiveIterations(const RenderSettings& settings) noexcept;

/// True when going from `from` to `to` needs the iteration data recomputed (rather than just
/// recoloured).
[[nodiscard]] bool needsRerender(const RenderSettings& from, const RenderSettings& to) noexcept;

}  // namespace mandelbrotter
