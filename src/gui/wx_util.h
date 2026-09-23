#pragma once

#include <string>
#include <string_view>

#include <wx/image.h>
#include <wx/string.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"

namespace mandelbrotter::gui
{

/// All text crosses the wx boundary as UTF-8, whatever the current locale is.
[[nodiscard]] wxString    toWx(std::string_view text);
[[nodiscard]] std::string fromWx(const wxString& text);

/// Copies an RgbImage into a wxImage (RGB, no alpha).
[[nodiscard]] wxImage toWxImage(const RgbImage& image);

/// "re + im i" with a fixed number of significant digits, e.g. "-0.7436 + 0.1318i".
[[nodiscard]] std::string formatComplex(Complex c, int precision = 10);
/// "re + im i" with the decimals the zoom calls for (six beyond the pixel size), at most 16 of
/// them followed by "..." when there would be more.
[[nodiscard]] std::string formatCenter(const BigComplex& c, double zoom);
/// "1x", "5000x", "1.2e+06x".
[[nodiscard]] std::string formatZoom(double zoom);

}  // namespace mandelbrotter::gui
