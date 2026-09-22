#include "gui/wx_util.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <span>

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

std::string formatComplex(Complex c, int precision)
{
    precision       = std::clamp(precision, 1, 17);
    const char sign = c.im < 0 ? '-' : '+';
    return std::format("{:.{}g} {} {:.{}g}i", c.re, precision, sign, std::abs(c.im), precision);
}

std::string formatZoom(double zoom)
{
    if (zoom < 1e5)
    {
        return std::format("{:.6g}x", zoom);
    }
    return std::format("{:.3g}x", zoom);
}

}  // namespace mandelbrotter::gui
