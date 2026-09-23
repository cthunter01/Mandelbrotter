#include "Mandelbrotter/ReferenceOrbit.h"

#include <cstddef>
#include <stop_token>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/BigFixed.h"
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
using mandelbrotter::ReferenceOrbit;

constexpr int kBits = 128;

/// The big-number orbit must reproduce the double one while the trajectory is well conditioned.
void expectMatchesDoubleOrbit(const FractalSpec& spec, Complex center, int maxIter,
                              std::string_view what)
{
    const ReferenceOrbit       ref(spec, BigComplex::fromComplex(center, kBits), maxIter);
    const std::vector<Complex> expected = mandelbrotter::orbit(spec, center, maxIter + 1);
    ASSERT_EQ(ref.length(), static_cast<int>(expected.size())) << what;
    for (std::size_t k = 0; k < expected.size(); ++k)
    {
        EXPECT_NEAR(ref.points()[k].re, expected[k].re, 1e-9) << what << " step " << k;
        EXPECT_NEAR(ref.points()[k].im, expected[k].im, 1e-9) << what << " step " << k;
    }
}

TEST(ReferenceOrbit, MatchesTheDoubleOrbitForEveryFamily)
{
    // Points with attracting behaviour, where rounding differences do not blow up.
    expectMatchesDoubleOrbit(FractalSpec{}, {-0.5, 0.5}, 200, "mandelbrot");
    expectMatchesDoubleOrbit(FractalSpec{.exponent = 3}, {0.1, 0.1}, 200, "cubic");
    expectMatchesDoubleOrbit(FractalSpec{.exponent = 5}, {0.1, 0.1}, 200, "quintic");
    expectMatchesDoubleOrbit(FractalSpec{.family = FractalFamily::BURNING_SHIP}, {0.1, 0.1}, 200,
                             "burning ship");
    expectMatchesDoubleOrbit(FractalSpec{.family = FractalFamily::TRICORN}, {-0.25, 0.0}, 200,
                             "tricorn");
    expectMatchesDoubleOrbit(FractalSpec{.julia = true, .seed = {-1.0, 0.0}}, {0.1, 0.2}, 200,
                             "julia");
    // Chaotic ones only for a few steps.
    expectMatchesDoubleOrbit(FractalSpec{}, {-0.75, 0.1}, 20, "mandelbrot boundary");
    expectMatchesDoubleOrbit(FractalSpec{.julia = true, .seed = {-0.8, 0.156}}, {0.1, 0.2}, 20,
                             "julia chaotic");
}

TEST(ReferenceOrbit, StopsAtTheFirstEscapedPoint)
{
    // 0 -> 1 -> 2 -> 5 -> 26 -> 677: six points, the last one past the bailout radius.
    const ReferenceOrbit ref(FractalSpec{}, BigComplex::fromComplex({1.0, 0.0}, kBits), 1000);
    EXPECT_TRUE(ref.escaped());
    EXPECT_FALSE(ref.cancelled());
    ASSERT_EQ(ref.length(), 6);
    EXPECT_DOUBLE_EQ(ref.points()[5].re, 677.0);
    EXPECT_GT(ref.points().back().normSquared(), mandelbrotter::kBailoutRadiusSquared);
    EXPECT_LT(ref.points()[4].normSquared(), mandelbrotter::kBailoutRadiusSquared);
}

TEST(ReferenceOrbit, RunsToMaxIterWhenBounded)
{
    const ReferenceOrbit ref(FractalSpec{}, BigComplex{kBits}, 50);
    EXPECT_FALSE(ref.escaped());
    EXPECT_EQ(ref.length(), 51);
    for (const Complex z : ref.points())
    {
        EXPECT_EQ(z, (Complex{0.0, 0.0}));
    }
    EXPECT_EQ(ReferenceOrbit(FractalSpec{}, BigComplex{kBits}, 0).length(), 1);
    EXPECT_EQ(ReferenceOrbit(FractalSpec{}, BigComplex{kBits}, -3).length(), 1);
}

TEST(ReferenceOrbit, JuliaModeStartsAtThePointItself)
{
    const FractalSpec    julia{.julia = true, .seed = {-1.0, 0.0}};
    const ReferenceOrbit ref(julia, BigComplex::fromComplex({0.1, 0.2}, kBits), 10);
    EXPECT_EQ(ref.points()[0], (Complex{0.1, 0.2}));
    EXPECT_EQ(ref.spec(), julia);
    // z1 = z0^2 + c = (0.01 - 0.04 - 1, 0.04)
    EXPECT_NEAR(ref.points()[1].re, -1.03, 1e-12);
    EXPECT_NEAR(ref.points()[1].im, 0.04, 1e-12);
}

TEST(ReferenceOrbit, CancellationKeepsWhatItHas)
{
    // Not const: the standard's request_stop() is non-const (libc++, MSVC); libstdc++'s const one
    // is an extension, which is why clang-tidy on Linux would otherwise ask for const here.
    std::stop_source source;  // NOLINT(misc-const-correctness)
    source.request_stop();
    const ReferenceOrbit ref(FractalSpec{}, BigComplex::fromComplex({-0.5, 0.5}, kBits), 1000,
                             source.get_token());
    EXPECT_TRUE(ref.cancelled());
    EXPECT_FALSE(ref.escaped());
    EXPECT_EQ(ref.length(), 1);
}

TEST(ReferenceOrbit, UsesThePrecisionOfItsCenter)
{
    // -2 is in the set (0, -2, 2, 2, ...); a hair to the left of it is not, but the hair is far
    // below what a double can hold, so only a big-number orbit sees the escape.
    const auto hair = BigFixed::fromDecimal("-2.000000000000000000000000000001", kBits);
    ASSERT_TRUE(hair.has_value());
    const ReferenceOrbit precise(FractalSpec{}, BigComplex{*hair, BigFixed{kBits}}, 400);
    EXPECT_TRUE(precise.escaped());
    EXPECT_GT(precise.length(), 40);
    EXPECT_LT(precise.length(), 80);

    const ReferenceOrbit rounded(FractalSpec{}, BigComplex::fromComplex({-2.0, 0.0}, kBits), 400);
    EXPECT_FALSE(rounded.escaped());
    EXPECT_EQ(rounded.length(), 401);
}

}  // namespace
