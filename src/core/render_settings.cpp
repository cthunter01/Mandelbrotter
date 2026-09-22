#include "Mandelbrotter/render_settings.h"

#include <algorithm>
#include <cmath>

namespace mandelbrotter
{

int autoIterationsFor(double zoom, int base) noexcept
{
    base = std::clamp(base, kMinIterations, kMaxIterations);
    if (!(zoom > 1.0))
    {
        return base;
    }
    const double extra = kIterationsPerZoomDoubling * std::log2(zoom);
    const double total =
        std::min(static_cast<double>(base) + extra, static_cast<double>(kMaxIterations));
    return static_cast<int>(total);
}

int effectiveIterations(const RenderSettings& settings) noexcept
{
    const int base = std::clamp(settings.maxIterations, kMinIterations, kMaxIterations);
    return settings.autoIterations ? autoIterationsFor(settings.view.zoom, base) : base;
}

bool needsRerender(const RenderSettings& from, const RenderSettings& to) noexcept
{
    return from.fractal != to.fractal || from.view != to.view ||
           effectiveIterations(from) != effectiveIterations(to);
}

}  // namespace mandelbrotter
