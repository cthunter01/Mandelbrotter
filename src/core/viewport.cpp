#include "Mandelbrotter/viewport.h"

#include <algorithm>
#include <cmath>

#include "Mandelbrotter/geometry.h"

namespace mandelbrotter
{

double clampZoom(double zoom) noexcept
{
    if (std::isnan(zoom))
    {
        return 1.0;
    }
    return std::clamp(zoom, kMinZoom, kMaxZoom);
}

Viewport::Viewport(ViewSpec view, PixelSize size) noexcept
  : m_view{view.center, clampZoom(view.zoom)},
    m_size{std::max(size.width, 1), std::max(size.height, 1)}
{
}

double Viewport::unitsPerPixel() const noexcept
{
    const int shortSide = std::min(m_size.width, m_size.height);
    return kUnitsAcrossShortSide / (m_view.zoom * shortSide);
}

Complex Viewport::toComplex(double px, double py) const noexcept
{
    const double s          = unitsPerPixel();
    const double halfWidth  = m_size.width / 2.0;
    const double halfHeight = m_size.height / 2.0;
    return {m_view.center.re + (px - halfWidth) * s, m_view.center.im - (py - halfHeight) * s};
}

Complex Viewport::pixelCenter(PixelPoint p) const noexcept
{
    return toComplex(p.x + 0.5, p.y + 0.5);
}

PixelPoint Viewport::toPixel(Complex c) const noexcept
{
    const double s  = unitsPerPixel();
    const double px = (c.re - m_view.center.re) / s + m_size.width / 2.0;
    const double py = -(c.im - m_view.center.im) / s + m_size.height / 2.0;
    return {static_cast<int>(std::floor(px)), static_cast<int>(std::floor(py))};
}

Viewport Viewport::zoomedAt(PixelPoint anchor, double factor) const noexcept
{
    const double  newZoom   = clampZoom(m_view.zoom * factor);
    const double  effective = newZoom / m_view.zoom;
    const Complex p         = pixelCenter(anchor);
    const Complex newCenter = p + (m_view.center - p) / effective;
    return {{newCenter, newZoom}, m_size};
}

Viewport Viewport::panned(int dx, int dy) const noexcept
{
    const double  s = unitsPerPixel();
    const Complex newCenter{m_view.center.re - dx * s, m_view.center.im + dy * s};
    return {{newCenter, m_view.zoom}, m_size};
}

Viewport Viewport::zoomedToRect(PixelRect rect) const noexcept
{
    if (rect.empty())
    {
        return *this;
    }
    const Complex newCenter = toComplex(rect.x + rect.width / 2.0, rect.y + rect.height / 2.0);
    const double  fraction  = std::max(static_cast<double>(rect.width) / m_size.width,
                                       static_cast<double>(rect.height) / m_size.height);
    return {{newCenter, clampZoom(m_view.zoom / fraction)}, m_size};
}

Viewport Viewport::resized(PixelSize size) const noexcept
{
    return {m_view, size};
}

}  // namespace mandelbrotter
