#pragma once

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter
{

/// Complex units spanned by the shorter window side at zoom 1.
inline constexpr double kUnitsAcrossShortSide = 3.0;
inline constexpr double kMinZoom              = 0.1;
/// The deepest zoom. Pixel offsets and the perturbation deltas are doubles, and at 1e300 they sit
/// just above the smallest normal double (a pixel is about 1e-303 wide).
inline constexpr double kMaxZoom = 1e300;
/// Bits the view centre keeps beyond what the pixel grid at its zoom can tell apart.
inline constexpr int kCenterGuardBits = 64;
/// toPixel clamps its result to this magnitude: far-off points stay representable as ints.
inline constexpr double kPixelCoordinateLimit = 1e9;

[[nodiscard]] double clampZoom(double zoom) noexcept;

/// The precision (fraction bits) a view centre carries at `zoom`: one bit per doubling of the zoom
/// plus the guard bits, so every pixel position is exact to a tiny fraction of its size.
[[nodiscard]] int fractionBitsFor(double zoom) noexcept;
/// Decimal places that reproduce a centre of fractionBitsFor(zoom) bits exactly.
[[nodiscard]] int centerDecimalsFor(double zoom);

/// Resolution-independent description of what is on screen: stored in bookmarks and given on the
/// command line. The centre is a big number so that a deep view keeps its place; Viewport gives it
/// the precision its zoom calls for (fractionBitsFor).
struct ViewSpec
{
    BigComplex center{-0.5, 0.0};
    double     zoom{1.0};

    bool operator==(const ViewSpec&) const = default;
};

/// A ViewSpec projected onto a pixel grid. Pixel (x, y) covers [x, x + 1) x [y, y + 1); y grows
/// downwards.
///
/// Positions come in two flavours: doubles relative to the centre (offsets), which are all a kernel
/// needs and stay accurate at any zoom, and absolute positions, either as BigComplex (always
/// accurate) or as doubles (only while the zoom is shallow enough for doubles to tell pixels
/// apart).
class Viewport
{
public:
    Viewport(const ViewSpec& view, PixelSize size);

    [[nodiscard]] const ViewSpec& view() const noexcept { return m_view; }
    [[nodiscard]] PixelSize       size() const noexcept { return m_size; }

    /// Complex units per pixel.
    [[nodiscard]] double unitsPerPixel() const noexcept;

    /// Continuous pixel coordinates -> offset from the centre, in complex units.
    [[nodiscard]] Complex offsetFromCenter(double px, double py) const noexcept;
    /// Continuous pixel coordinates -> complex plane, as doubles.
    [[nodiscard]] Complex toComplex(double px, double py) const noexcept;
    /// The complex number at the centre of a pixel (what the renderer samples), as doubles.
    [[nodiscard]] Complex pixelCenter(PixelPoint p) const noexcept;
    /// Continuous pixel coordinates -> complex plane, at the view's full precision.
    [[nodiscard]] BigComplex toBig(double px, double py) const;
    [[nodiscard]] BigComplex pixelCenterBig(PixelPoint p) const;
    /// Complex plane -> the pixel containing that point (may lie outside the viewport; the
    /// coordinates are clamped to +-kPixelCoordinateLimit).
    [[nodiscard]] PixelPoint toPixel(Complex c) const noexcept;
    [[nodiscard]] PixelPoint toPixel(const BigComplex& c) const;

    /// Zoom by `factor` (> 1 zooms in) keeping the complex number under `anchor` at the same pixel.
    [[nodiscard]] Viewport zoomedAt(PixelPoint anchor, double factor) const;
    /// Move the image by (dx, dy) pixels (a drag to the right is dx > 0).
    [[nodiscard]] Viewport panned(int dx, int dy) const;
    /// Zoom so that `rect` fills the viewport, preserving aspect ratio (the rect is fully visible).
    [[nodiscard]] Viewport zoomedToRect(PixelRect rect) const;
    /// Same view (centre and zoom) on a different pixel grid.
    [[nodiscard]] Viewport resized(PixelSize size) const;

private:
    /// The centre moved by a double offset, at the precision `zoom` calls for.
    [[nodiscard]] BigComplex shiftedCenter(Complex offset, double zoom) const;
    /// The pixel at a double offset from the centre.
    [[nodiscard]] PixelPoint pixelAtOffset(Complex offset) const noexcept;

    ViewSpec  m_view;
    PixelSize m_size;
    Complex   m_centerApprox;  ///< m_view.center as doubles, for the shallow-zoom paths
};

}  // namespace mandelbrotter
