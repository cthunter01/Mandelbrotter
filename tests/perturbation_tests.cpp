#include "Mandelbrotter/perturbation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

#include <gtest/gtest.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/BigFixed.h"
#include "Mandelbrotter/ReferenceOrbit.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/kernel.h"

namespace
{

using mandelbrotter::BigComplex;
using mandelbrotter::BigFixed;
using mandelbrotter::Complex;
using mandelbrotter::FractalFamily;
using mandelbrotter::FractalSpec;
using mandelbrotter::IterationResult;
using mandelbrotter::PixelPoint;
using mandelbrotter::PixelSize;
using mandelbrotter::ReferenceOrbit;
using mandelbrotter::Viewport;

constexpr int       kGrid = 48;
constexpr PixelSize kSize{kGrid, kGrid};
constexpr int       kMaxIter = 500;

/// Two ways of computing the same pixel agree unless rounding tipped a chaotic orbit.
bool sameResult(IterationResult a, IterationResult b)
{
    return a.interior == b.interior &&
           std::abs(a.smoothIter - b.smoothIter) <= 1e-6 * std::max(1.0, a.smoothIter);
}

/// The exact result for a point: its own big-number orbit, at whatever precision `point` carries.
IterationResult exactResult(const FractalSpec& spec, const BigComplex& point, int maxIter)
{
    const ReferenceOrbit orbit(spec, point, maxIter);
    return mandelbrotter::smoothIterationResult(
        orbit.length() - 1, orbit.points().back().normSquared(), orbit.escaped(), spec.exponent);
}

/// Over a 6x6 sample of a kGrid x kGrid view at `zoom` around `center`: how many pixels direct
/// iteration and perturbation each get wrong against a 512-bit orbit of the pixel. A chaotic orbit
/// amplifies rounding until any double computation is off, so neither count is zero in general;
/// perturbation, whose rounding is relative to the small delta, must not be the worse of the two.
struct Disagreements
{
    int direct{};
    int perturbed{};
};

Disagreements disagreements(const FractalSpec& spec, Complex center, double zoom)
{
    constexpr int        kSample    = 6;
    constexpr int        kIter      = 200;
    constexpr int        kTruthBits = 512;
    const Viewport       vp{{{center.re, center.im}, zoom}, kSize};
    const ReferenceOrbit ref(
        spec, BigComplex::fromComplex(center, mandelbrotter::fractionBitsFor(zoom)), kIter);
    Disagreements d;
    for (int j = 0; j < kSample; ++j)
    {
        for (int i = 0; i < kSample; ++i)
        {
            const PixelPoint      p{i * kGrid / kSample + kGrid / (2 * kSample),
                                    j * kGrid / kSample + kGrid / (2 * kSample)};
            const IterationResult truth =
                exactResult(spec, vp.pixelCenterBig(p).withFractionBits(kTruthBits), kIter);
            const IterationResult direct =
                mandelbrotter::iteratePixel(spec, vp.pixelCenter(p), kIter);
            const IterationResult perturbed = mandelbrotter::iteratePerturbed(
                ref, vp.offsetFromCenter(p.x + 0.5, p.y + 0.5), kIter);
            d.direct += sameResult(truth, direct) ? 0 : 1;
            d.perturbed += sameResult(truth, perturbed) ? 0 : 1;
        }
    }
    return d;
}

TEST(Perturbation, DiffAbsIsTheExactDifferenceOfAbsoluteValues)
{
    for (const double c : {3.0, -3.0, 0.0, 0.5, -0.5})
    {
        for (const double d : {1.0, -1.0, 5.0, -5.0, 0.0, 2.5, -2.5, 3.0, -3.0})
        {
            EXPECT_DOUBLE_EQ(mandelbrotter::diffAbs(c, d), std::abs(c + d) - std::abs(c))
                << c << " " << d;
        }
    }
}

TEST(Perturbation, NoLessAccurateThanDirectIterationForEveryFamily)
{
    struct Case
    {
        std::string_view name;
        FractalSpec      spec;
        Complex          center;
    };
    const auto cases = std::to_array<Case>({
        {"mandelbrot", FractalSpec{}, {-0.75, 0.1}},
        {"mandelbrot antenna", FractalSpec{}, {-1.75, 0.0}},
        {"cubic", FractalSpec{.exponent = 3}, {-0.38, 0.05}},
        {"quintic", FractalSpec{.exponent = 5}, {-0.7, 0.2}},
        {"burning ship", FractalSpec{.family = FractalFamily::BURNING_SHIP}, {-1.75, -0.03}},
        {"burning ship chaotic",
         FractalSpec{.family = FractalFamily::BURNING_SHIP},
         {-1.62, -0.02}},
        {"tricorn", FractalSpec{.family = FractalFamily::TRICORN}, {-1.3, 0.2}},
        {"julia", FractalSpec{.julia = true, .seed = {-0.8, 0.156}}, {0.1, 0.2}},
    });
    for (const Case& c : cases)
    {
        for (const double zoom : {1e3, 1e8})
        {
            const Disagreements d = disagreements(c.spec, c.center, zoom);
            EXPECT_LE(d.perturbed, d.direct) << c.name << " at " << zoom;
        }
    }
}

TEST(Perturbation, RebasingLetsAnyReferenceServeAnyPixel)
{
    // A reference that escapes after five steps, and pixels spread over the whole set: every
    // pixel rebases over and over, and still comes out right.
    const FractalSpec    spec{};
    const Complex        refCenter{1.0, 0.0};
    const ReferenceOrbit ref(spec, BigComplex::fromComplex(refCenter, 64), kMaxIter);
    ASSERT_TRUE(ref.escaped());
    ASSERT_EQ(ref.length(), 6);
    const Viewport vp{{{-0.5, 0.0}, 1.0}, kSize};
    int            bad = 0;
    for (int y = 0; y < kGrid; ++y)
    {
        for (int x = 0; x < kGrid; ++x)
        {
            const Complex         c      = vp.pixelCenter({x, y});
            const IterationResult direct = mandelbrotter::iteratePixel(spec, c, kMaxIter);
            const IterationResult perturbed =
                mandelbrotter::iteratePerturbed(ref, c - refCenter, kMaxIter);
            bad += sameResult(direct, perturbed) ? 0 : 1;
        }
    }
    EXPECT_LE(bad, kGrid * kGrid / 50);

    // The same in Julia mode, where the reference is an orbit of z0 = 0.
    const FractalSpec    julia{.julia = true, .seed = {-0.8, 0.156}};
    const ReferenceOrbit juliaRef(julia, BigComplex{64}, kMaxIter);
    bad = 0;
    for (int y = 0; y < kGrid; ++y)
    {
        for (int x = 0; x < kGrid; ++x)
        {
            const Complex         z0     = vp.pixelCenter({x, y});
            const IterationResult direct = mandelbrotter::iteratePixel(julia, z0, kMaxIter);
            const IterationResult perturbed =
                mandelbrotter::iteratePerturbed(juliaRef, z0, kMaxIter);
            bad += sameResult(direct, perturbed) ? 0 : 1;
        }
    }
    EXPECT_LE(bad, kGrid * kGrid / 50);
}

TEST(Perturbation, MatchesBigNumberIterationFarBeyondDoublePrecision)
{
    // At zoom 1e50 every pixel is the same double; the truth is each pixel's own big-number orbit.
    constexpr double kZoom = 1e50;
    constexpr int    kIter = 400;
    const int        bits  = mandelbrotter::fractionBitsFor(kZoom);
    struct Case
    {
        std::string_view name;
        FractalSpec      spec;
        std::string_view re;
        std::string_view im;
    };
    const auto cases = std::to_array<Case>({
        {"mandelbrot at -2", FractalSpec{}, "-2", "0"},
        {"cubic", FractalSpec{.exponent = 3}, "-0.39", "0"},
        {"quintic", FractalSpec{.exponent = 5}, "-0.8", "0.1"},
        {"burning ship at -2", FractalSpec{.family = FractalFamily::BURNING_SHIP}, "-2", "0"},
        {"tricorn at -2", FractalSpec{.family = FractalFamily::TRICORN}, "-2", "0"},
        {"julia inside", FractalSpec{.julia = true, .seed = {-1.0, 0.0}}, "0.1", "0.2"},
        {"julia outside", FractalSpec{.julia = true, .seed = {-1.0, 0.0}}, "1.5", "0.5"},
    });
    for (const Case& c : cases)
    {
        const BigComplex     center{*BigFixed::fromDecimal(c.re, bits),
                                    *BigFixed::fromDecimal(c.im, bits)};
        const ReferenceOrbit ref(c.spec, center, kIter);
        const Viewport       vp{{center, kZoom}, kSize};
        for (const PixelPoint p : {PixelPoint{0, 0}, PixelPoint{kGrid - 1, kGrid - 1},
                                   PixelPoint{kGrid / 2, kGrid / 2}, PixelPoint{7, 40}})
        {
            const IterationResult expected = exactResult(c.spec, vp.pixelCenterBig(p), kIter);
            const IterationResult actual   = mandelbrotter::iteratePerturbed(
                ref, vp.offsetFromCenter(p.x + 0.5, p.y + 0.5), kIter);
            EXPECT_EQ(actual.interior, expected.interior)
                << c.name << " pixel " << p.x << "," << p.y;
            EXPECT_NEAR(actual.smoothIter, expected.smoothIter,
                        1e-6 * std::max(1.0, expected.smoothIter))
                << c.name << " pixel " << p.x << "," << p.y;
        }
    }
}

TEST(Perturbation, DegenerateReferencesDoNotStep)
{
    // A lone point (maxIter 0) cannot be stepped along: the pixel is judged where it stands. In
    // Julia mode that is the pixel itself; in Mandelbrot mode every pixel starts at zero.
    const FractalSpec    julia{.julia = true, .seed = {0.0, 0.0}};
    const ReferenceOrbit lone(julia, BigComplex{64}, 0);
    ASSERT_EQ(lone.length(), 1);
    EXPECT_TRUE(mandelbrotter::iteratePerturbed(lone, {0.1, 0.1}, 100).interior);
    EXPECT_FALSE(mandelbrotter::iteratePerturbed(lone, {1000.0, 0.0}, 100).interior);
    const ReferenceOrbit loneMandelbrot(FractalSpec{}, BigComplex{64}, 0);
    EXPECT_TRUE(mandelbrotter::iteratePerturbed(loneMandelbrot, {1000.0, 0.0}, 100).interior);
    // maxIter 0 behaves like iterate().
    const ReferenceOrbit ref(FractalSpec{}, BigComplex{64}, 10);
    EXPECT_TRUE(mandelbrotter::iteratePerturbed(ref, {1.0, 0.0}, 0).interior);
    EXPECT_TRUE(mandelbrotter::iteratePerturbed(ref, {1.0, 0.0}, -5).interior);
}

}  // namespace
