#include "gui/wx_util.h"

#include <algorithm>
#include <span>
#include <string>
#include <string_view>

namespace mandelbrotter::gui
{

wxString toWx(std::string_view text)
{
    return wxString::FromUTF8(text.data(), text.size());
}

std::string fromWx(const wxString& text)
{
    return text.utf8_string();
}

wxImage toWxImage(const RgbImage& image)
{
    if (image.size().empty())
    {
        return {};
    }
    wxImage                        out(image.width, image.height, false);
    const std::span<unsigned char> target(out.GetData(), image.pixels.size());
    std::ranges::copy(image.pixels, target.begin());
    return out;
}

wxPoint toWx(PixelPoint point)
{
    return {point.x, point.y};
}

PixelPoint fromWx(wxPoint point)
{
    return {point.x, point.y};
}

wxRect toWx(PixelRect rect)
{
    return {rect.x, rect.y, rect.width, rect.height};
}

PixelRect fromWx(const wxRect& rect)
{
    return {rect.x, rect.y, rect.width, rect.height};
}

PixelSize fromWx(wxSize size)
{
    return {size.x, size.y};
}

}  // namespace mandelbrotter::gui
