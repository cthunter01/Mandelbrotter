#include "Mandelbrotter/Viewport.h"

#include <algorithm>
#include <cmath>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/BigFixed.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter
{

namespace
{

int clampedPixel(double coordinate) noexcept
{
    return static_cast<int>(
        std::floor(std::clamp(coordinate, -kPixelCoordinateLimit, kPixelCoordinateLimit)));
}

}  // namespace

double clampZoom(double zoom) noexcept
{
    if (std::isnan(zoom))
    {
        return 1.0;
    }
    return std::clamp(zoom, kMinZoom, kMaxZoom);
}

int fractionBitsFor(double zoom) noexcept
{
    const double doublings = std::log2(std::max(clampZoom(zoom), 1.0));
    return static_cast<int>(std::ceil(doublings)) + kCenterGuardBits;
}

int centerDecimalsFor(double zoom)
{
    // 10^-digits must be well below 2^-bits, so that printing (rounded) and parsing (rounded) both
    // land back on the same binary value.
    const int bits = BigFixed{fractionBitsFor(zoom)}.fractionBits();
    return static_cast<int>(std::ceil(bits * std::log10(2.0))) + 1;
}

Viewport::Viewport(const ViewSpec& view, PixelSize size)
  : m_view{view.center, clampZoom(view.zoom)},
    m_size{std::max(size.width, 1), std::max(size.height, 1)},
    m_centerApprox(m_view.center.approx())
{
}

double Viewport::unitsPerPixel() const noexcept
{
    const int shortSide = std::min(m_size.width, m_size.height);
    return kUnitsAcrossShortSide / (m_view.zoom * shortSide);
}

Complex Viewport::offsetFromCenter(double px, double py) const noexcept
{
    const double s = unitsPerPixel();
    return {(px - m_size.width / 2.0) * s, -(py - m_size.height / 2.0) * s};
}

Complex Viewport::toComplex(double px, double py) const noexcept
{
    return m_centerApprox + offsetFromCenter(px, py);
}

Complex Viewport::pixelCenter(PixelPoint p) const noexcept
{
    return toComplex(p.x + 0.5, p.y + 0.5);
}

BigComplex Viewport::toBig(double px, double py) const
{
    return shiftedCenter(offsetFromCenter(px, py), m_view.zoom);
}

BigComplex Viewport::pixelCenterBig(PixelPoint p) const
{
    return toBig(p.x + 0.5, p.y + 0.5);
}

PixelPoint Viewport::pixelAtOffset(Complex offset) const noexcept
{
    const double s = unitsPerPixel();
    return {clampedPixel(offset.re / s + m_size.width / 2.0),
            clampedPixel(-offset.im / s + m_size.height / 2.0)};
}

PixelPoint Viewport::toPixel(Complex c) const noexcept
{
    return pixelAtOffset(c - m_centerApprox);
}

PixelPoint Viewport::toPixel(const BigComplex& c) const
{
    // Only the difference needs the full precision; as a double it is a plain offset.
    return pixelAtOffset((c - m_view.center).approx());
}

BigComplex Viewport::shiftedCenter(Complex offset, double zoom) const
{
    const int bits = fractionBitsFor(zoom);
    return m_view.center.withFractionBits(bits) + BigComplex::fromComplex(offset, bits);
}

Viewport Viewport::zoomedAt(PixelPoint anchor, double factor) const
{
    const double newZoom   = clampZoom(m_view.zoom * factor);
    const double effective = newZoom / m_view.zoom;
    // The anchor stays under the same pixel: the centre moves toward it by the share of the
    // distance the zoom takes away.
    const Complex offset = offsetFromCenter(anchor.x + 0.5, anchor.y + 0.5);
    return {{shiftedCenter(offset * (1.0 - 1.0 / effective), newZoom), newZoom}, m_size};
}

Viewport Viewport::panned(int dx, int dy) const
{
    const double s = unitsPerPixel();
    return {{shiftedCenter({-dx * s, dy * s}, m_view.zoom), m_view.zoom}, m_size};
}

Viewport Viewport::zoomedToRect(PixelRect rect) const
{
    if (rect.empty())
    {
        return *this;
    }
    const Complex offset = offsetFromCenter(rect.x + rect.width / 2.0, rect.y + rect.height / 2.0);
    const double  fraction = std::max(static_cast<double>(rect.width) / m_size.width,
                                      static_cast<double>(rect.height) / m_size.height);
    const double  newZoom  = clampZoom(m_view.zoom / fraction);
    return {{shiftedCenter(offset, newZoom), newZoom}, m_size};
}

Viewport Viewport::resized(PixelSize size) const
{
    return {m_view, size};
}

}  // namespace mandelbrotter
