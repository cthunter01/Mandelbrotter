#include "gui/wx_util.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <span>
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

std::string formatComplex(Complex c, int precision)
{
    precision       = std::clamp(precision, 1, 17);
    const char sign = c.im < 0 ? '-' : '+';
    return std::format("{:.{}g} {} {:.{}g}i", c.re, precision, sign, std::abs(c.im), precision);
}

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

}  // namespace mandelbrotter::gui
