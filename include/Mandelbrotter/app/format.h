#pragma once

#include <string>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/RenderSettings.h"

namespace mandelbrotter::app
{

/// "re + im i" with the decimals the zoom calls for (six beyond the pixel size), at most 16 of
/// them followed by "..." when there would be more.
[[nodiscard]] std::string formatCenter(const BigComplex& c, double zoom);
/// "1x", "5000x", "1.2e+06x".
[[nodiscard]] std::string formatZoom(double zoom);
/// The text of one Julia seed field: ten significant digits, e.g. "-0.8".
[[nodiscard]] std::string formatSeedComponent(double value);
/// The name Add bookmark suggests: "Mandelbrot at 5000x".
[[nodiscard]] std::string defaultBookmarkName(const RenderSettings& settings);

}  // namespace mandelbrotter::app
