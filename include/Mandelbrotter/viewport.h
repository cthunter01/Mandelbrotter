#pragma once

#include "Mandelbrotter/geometry.h"

namespace mandelbrotter
{

/// Complex units spanned by the shorter window side at zoom 1.
inline constexpr double kUnitsAcrossShortSide = 3.0;
inline constexpr double kMinZoom              = 0.1;
/// Beyond this, double precision runs out (pixels become identical).
inline constexpr double kMaxZoom = 1e12;

[[nodiscard]] double clampZoom(double zoom) noexcept;

/// Resolution-independent description of what is on screen: stored in bookmarks and given on the
/// command line.
struct ViewSpec
{
    Complex center{-0.5, 0.0};
    double  zoom{1.0};

    bool operator==(const ViewSpec&) const = default;
};

/// A ViewSpec projected onto a pixel grid. Pixel (x, y) covers [x, x + 1) x [y, y + 1); y grows
/// downwards.
class Viewport
{
public:
    Viewport(ViewSpec view, PixelSize size) noexcept;

    [[nodiscard]] const ViewSpec& view() const noexcept { return m_view; }
    [[nodiscard]] PixelSize       size() const noexcept { return m_size; }

    /// Complex units per pixel.
    [[nodiscard]] double unitsPerPixel() const noexcept;

    /// Continuous pixel coordinates -> complex plane.
    [[nodiscard]] Complex toComplex(double px, double py) const noexcept;
    /// The complex number at the centre of a pixel (what the renderer samples).
    [[nodiscard]] Complex pixelCenter(PixelPoint p) const noexcept;
    /// Complex plane -> the pixel containing that point (may lie outside the viewport).
    [[nodiscard]] PixelPoint toPixel(Complex c) const noexcept;

    /// Zoom by `factor` (> 1 zooms in) keeping the complex number under `anchor` at the same pixel.
    [[nodiscard]] Viewport zoomedAt(PixelPoint anchor, double factor) const noexcept;
    /// Move the image by (dx, dy) pixels (a drag to the right is dx > 0).
    [[nodiscard]] Viewport panned(int dx, int dy) const noexcept;
    /// Zoom so that `rect` fills the viewport, preserving aspect ratio (the rect is fully visible).
    [[nodiscard]] Viewport zoomedToRect(PixelRect rect) const noexcept;
    /// Same view (centre and zoom) on a different pixel grid.
    [[nodiscard]] Viewport resized(PixelSize size) const noexcept;

private:
    ViewSpec  m_view;
    PixelSize m_size;
};

}  // namespace mandelbrotter
