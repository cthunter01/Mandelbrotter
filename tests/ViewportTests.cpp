#include "Mandelbrotter/Viewport.h"

#include <cmath>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/BigFixed.h"
#include "Mandelbrotter/geometry.h"

namespace
{

using mandelbrotter::BigComplex;
using mandelbrotter::BigFixed;
using mandelbrotter::Complex;
using mandelbrotter::PixelPoint;
using mandelbrotter::PixelRect;
using mandelbrotter::PixelSize;
using mandelbrotter::Viewport;
using mandelbrotter::ViewSpec;

constexpr PixelSize kSize{800, 600};

TEST(Viewport, ShortSideSpansThreeUnitsAtZoomOne)
{
    const Viewport vp{{{0.0, 0.0}, 1.0}, kSize};
    EXPECT_DOUBLE_EQ(vp.unitsPerPixel() * 600, 3.0);
}

TEST(Viewport, CenterPixelMapsToCenter)
{
    const Viewport vp{{{-0.5, 0.25}, 2.0}, kSize};
    const Complex  c = vp.toComplex(400.0, 300.0);
    EXPECT_DOUBLE_EQ(c.re, -0.5);
    EXPECT_DOUBLE_EQ(c.im, 0.25);
}

TEST(Viewport, ImaginaryAxisPointsUp)
{
    const Viewport vp{{{0.0, 0.0}, 1.0}, kSize};
    EXPECT_GT(vp.toComplex(400.0, 0.0).im, 0.0);
    EXPECT_LT(vp.toComplex(400.0, 600.0).im, 0.0);
    EXPECT_LT(vp.toComplex(0.0, 300.0).re, 0.0);
}

TEST(Viewport, PixelRoundTrip)
{
    const Viewport vp{{{-0.75, 0.1}, 37.0}, kSize};
    for (const PixelPoint p : {PixelPoint{0, 0}, PixelPoint{799, 599}, PixelPoint{123, 456}})
    {
        EXPECT_EQ(vp.toPixel(vp.pixelCenter(p)), p);
    }
}

TEST(Viewport, ZoomAtAnchorKeepsAnchorFixed)
{
    const Viewport   vp{{{-0.5, 0.0}, 1.0}, kSize};
    const PixelPoint anchor{150, 500};
    const Complex    before = vp.pixelCenter(anchor);
    const Viewport   zoomed = vp.zoomedAt(anchor, 4.0);
    const Complex    after  = zoomed.pixelCenter(anchor);
    EXPECT_NEAR(after.re, before.re, 1e-12);
    EXPECT_NEAR(after.im, before.im, 1e-12);
    EXPECT_DOUBLE_EQ(zoomed.view().zoom, 4.0);
}

TEST(Viewport, ZoomOutAtAnchorKeepsAnchorFixed)
{
    const Viewport   vp{{{0.3, -0.2}, 1000.0}, kSize};
    const PixelPoint anchor{10, 20};
    const Complex    before = vp.pixelCenter(anchor);
    const Complex    after  = vp.zoomedAt(anchor, 0.5).pixelCenter(anchor);
    EXPECT_NEAR(after.re, before.re, 1e-12);
    EXPECT_NEAR(after.im, before.im, 1e-12);
}

TEST(Viewport, ZoomIsClamped)
{
    const Viewport vp{{{0.0, 0.0}, 1.0}, kSize};
    EXPECT_DOUBLE_EQ(vp.zoomedAt({0, 0}, 1e-9).view().zoom, mandelbrotter::kMinZoom);
    EXPECT_DOUBLE_EQ(vp.zoomedAt({0, 0}, 1e30).view().zoom, mandelbrotter::kMaxZoom);
    EXPECT_DOUBLE_EQ(mandelbrotter::clampZoom(std::numeric_limits<double>::quiet_NaN()), 1.0);
}

TEST(Viewport, PanMovesImageWithTheDrag)
{
    const Viewport vp{{{0.0, 0.0}, 1.0}, kSize};
    const Complex  under = vp.pixelCenter({100, 100});
    // Drag by (+50, +30): whatever was at (100, 100) is now at (150, 130).
    const Viewport moved = vp.panned(50, 30);
    const Complex  now   = moved.pixelCenter({150, 130});
    EXPECT_NEAR(now.re, under.re, 1e-12);
    EXPECT_NEAR(now.im, under.im, 1e-12);
}

TEST(Viewport, ZoomToRectCentersAndFitsRect)
{
    const Viewport  vp{{{0.0, 0.0}, 1.0}, kSize};
    const PixelRect rect{100, 200, 400, 100};  // wider than tall relative to the 4:3 viewport
    const Complex   rectCenter = vp.toComplex(300.0, 250.0);
    const Viewport  zoomed     = vp.zoomedToRect(rect);
    EXPECT_NEAR(zoomed.view().center.approx().re, rectCenter.re, 1e-12);
    EXPECT_NEAR(zoomed.view().center.approx().im, rectCenter.im, 1e-12);
    // The limiting side is the width (400 / 800 = 0.5 vs 100 / 600 = 0.167): zoom doubles.
    EXPECT_DOUBLE_EQ(zoomed.view().zoom, 2.0);
    // The rect's left and right edges land on the viewport's left and right edges.
    EXPECT_NEAR(zoomed.toComplex(0.0, 300.0).re, vp.toComplex(100.0, 300.0).re, 1e-12);
    EXPECT_NEAR(zoomed.toComplex(800.0, 300.0).re, vp.toComplex(500.0, 300.0).re, 1e-12);
}

TEST(Viewport, ZoomToEmptyRectIsNoOp)
{
    const Viewport vp{{{0.1, 0.2}, 3.0}, kSize};
    EXPECT_EQ(vp.zoomedToRect({10, 10, 0, 5}).view(), vp.view());
}

TEST(Viewport, ResizeKeepsCenterAndZoom)
{
    const Viewport vp{{{0.1, 0.2}, 3.0}, kSize};
    const Viewport resized = vp.resized({1600, 400});
    EXPECT_EQ(resized.view(), vp.view());
    EXPECT_EQ(resized.size(), (PixelSize{1600, 400}));
    EXPECT_DOUBLE_EQ(resized.unitsPerPixel() * 400, 3.0 / 3.0);
}

TEST(Viewport, DegenerateSizeIsClampedToOnePixel)
{
    const Viewport vp{{{0.0, 0.0}, 1.0}, {0, -5}};
    EXPECT_EQ(vp.size(), (PixelSize{1, 1}));
    EXPECT_TRUE(std::isfinite(vp.unitsPerPixel()));
}

TEST(Viewport, PrecisionFollowsTheZoom)
{
    EXPECT_EQ(mandelbrotter::fractionBitsFor(1.0), mandelbrotter::kCenterGuardBits);
    EXPECT_EQ(mandelbrotter::fractionBitsFor(0.5), mandelbrotter::kCenterGuardBits);
    EXPECT_EQ(mandelbrotter::fractionBitsFor(2.0), mandelbrotter::kCenterGuardBits + 1);
    EXPECT_EQ(mandelbrotter::fractionBitsFor(1e12), mandelbrotter::kCenterGuardBits + 40);
    // Decimals that reproduce that many bits (rounded up to limbs): 64 bits need 20, 128 need 39,
    // plus one for the rounding.
    EXPECT_EQ(mandelbrotter::centerDecimalsFor(1.0), 21);
    EXPECT_EQ(mandelbrotter::centerDecimalsFor(1e12), 40);
}

TEST(Viewport, BigPositionsKeepEveryDigit)
{
    const std::string re   = "-0.7436438870371587047521915061147740";
    const std::string im   = "0.1318259042053119704931320563851390";
    const int         bits = mandelbrotter::fractionBitsFor(mandelbrotter::kMaxZoom);
    const BigComplex  center{*BigFixed::fromDecimal(re, bits), *BigFixed::fromDecimal(im, bits)};
    const Viewport    vp{{center, mandelbrotter::kMaxZoom}, kSize};
    // The middle of the viewport is the centre itself, digits intact...
    const BigComplex middle = vp.toBig(400.0, 300.0);
    EXPECT_EQ(middle, center);
    EXPECT_EQ(middle.re.toDecimal(34), re);
    EXPECT_EQ(middle.im.toDecimal(34), im);
    // ...and one pixel to the right differs by exactly one pixel.
    const BigComplex right = vp.toBig(401.0, 300.0);
    EXPECT_EQ((right - center).re, BigFixed::fromDouble(vp.unitsPerPixel(), bits));
    EXPECT_TRUE((right - center).im.isZero());
}

TEST(Viewport, PanAndZoomMoveTheBigCenter)
{
    const Viewport   vp{{{0.25, -0.5}, 1e6}, kSize};
    const double     s    = vp.unitsPerPixel();
    const int        bits = mandelbrotter::fractionBitsFor(1e6);
    const BigComplex expectedPan =
        vp.view().center + BigComplex::fromComplex({-3 * s, 2 * s}, bits);
    EXPECT_EQ(vp.panned(3, 2).view().center, expectedPan);
    EXPECT_EQ(vp.panned(3, 2).view().center.fractionBits(), BigFixed{bits}.fractionBits());

    // Zooming keeps the anchor in place to far below a pixel, and the precision follows the zoom.
    const PixelPoint anchor{150, 500};
    const Viewport   zoomed    = vp.zoomedAt(anchor, 4.0);
    const BigComplex drift     = zoomed.pixelCenterBig(anchor) - vp.pixelCenterBig(anchor);
    const BigFixed   tolerance = BigFixed::fromDouble(s * 1e-9, bits);
    EXPECT_LT(drift.re.abs(), tolerance);
    EXPECT_LT(drift.im.abs(), tolerance);
    EXPECT_EQ(zoomed.view().center.fractionBits(),
              BigFixed{mandelbrotter::fractionBitsFor(4e6)}.fractionBits());
}

TEST(Viewport, BigPixelRoundTrip)
{
    const Viewport vp{{{-0.75, 0.1}, 1e12}, kSize};
    for (const PixelPoint p : {PixelPoint{0, 0}, PixelPoint{799, 599}, PixelPoint{123, 456}})
    {
        EXPECT_EQ(vp.toPixel(vp.pixelCenterBig(p)), p);
    }
}

TEST(Viewport, FarAwayPointsClampInsteadOfOverflowing)
{
    const Viewport vp{{{0.0, 0.0}, 1e12}, kSize};
    const auto     limit = static_cast<int>(mandelbrotter::kPixelCoordinateLimit);
    EXPECT_EQ(vp.toPixel(Complex{1e30, -1e30}), (PixelPoint{limit, limit}));
    EXPECT_EQ(vp.toPixel(Complex{-1e30, 1e30}), (PixelPoint{-limit, -limit}));
    EXPECT_EQ(vp.toPixel(BigComplex{1e10, 1e10}), (PixelPoint{limit, -limit}));
}

}  // namespace
