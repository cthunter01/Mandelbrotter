#pragma once

#include <string>
#include <string_view>

#include <wx/colour.h>
#include <wx/gdicmn.h>
#include <wx/image.h>
#include <wx/string.h>

#include "Mandelbrotter/app/ui_text.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"

namespace mandelbrotter::gui
{

/// All text crosses the wx boundary as UTF-8, whatever the current locale is.
[[nodiscard]] wxString    toWx(std::string_view text);
[[nodiscard]] std::string fromWx(const wxString& text);

/// Copies an RgbImage into a wxImage (RGB, no alpha).
[[nodiscard]] wxImage toWxImage(const RgbImage& image);

/// Points, sizes and rectangles in pixels, as the app layer and wx spell them.
[[nodiscard]] wxPoint    toWx(PixelPoint point);
[[nodiscard]] PixelPoint fromWx(wxPoint point);
[[nodiscard]] wxRect     toWx(PixelRect rect);
[[nodiscard]] PixelRect  fromWx(const wxRect& rect);
[[nodiscard]] PixelSize  fromWx(wxSize size);

[[nodiscard]] wxColour toWx(app::Rgba color);

}  // namespace mandelbrotter::gui
