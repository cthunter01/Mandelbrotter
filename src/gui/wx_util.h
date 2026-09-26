#pragma once

#include <string>
#include <string_view>

#include <wx/image.h>
#include <wx/string.h>

#include "Mandelbrotter/image.h"

namespace mandelbrotter::gui
{

/// All text crosses the wx boundary as UTF-8, whatever the current locale is.
[[nodiscard]] wxString    toWx(std::string_view text);
[[nodiscard]] std::string fromWx(const wxString& text);

/// Copies an RgbImage into a wxImage (RGB, no alpha).
[[nodiscard]] wxImage toWxImage(const RgbImage& image);

}  // namespace mandelbrotter::gui
