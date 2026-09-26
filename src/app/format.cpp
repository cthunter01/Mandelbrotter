#include "Mandelbrotter/app/format.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <string_view>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/fractal.h"

namespace mandelbrotter::app
{

std::string formatCenter(const BigComplex& c, double zoom)
{
    constexpr int kMaxDecimals = 16;
    constexpr int kBeyondPixel = 6;
    const int wanted = static_cast<int>(std::ceil(std::log10(std::max(zoom, 1.0)))) + kBeyondPixel;
    const int digits = std::min(wanted, kMaxDecimals);
    const std::string_view more = wanted > kMaxDecimals ? "..." : "";
    const char             sign = c.im.isNegative() ? '-' : '+';
    return std::format("{}{} {} {}{}i", c.re.toDecimal(digits), more, sign,
                       c.im.abs().toDecimal(digits), more);
}

std::string formatZoom(double zoom)
{
    if (zoom < 1e5)
    {
        return std::format("{:.6g}x", zoom);
    }
    return std::format("{:.3g}x", zoom);
}

std::string formatSeedComponent(double value)
{
    return std::format("{:.10g}", value);
}

std::string defaultBookmarkName(const RenderSettings& settings)
{
    return std::string(displayName(settings.fractal.family)) + " at " +
           formatZoom(settings.view.zoom);
}

}  // namespace mandelbrotter::app
