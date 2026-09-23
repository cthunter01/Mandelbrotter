#include "Mandelbrotter/BlaTable.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string_view>

#include <gtest/gtest.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/ReferenceOrbit.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace
{

using mandelbrotter::BigComplex;
using mandelbrotter::Bla;
using mandelbrotter::BlaTable;
using mandelbrotter::Complex;
using mandelbrotter::FractalFamily;
using mandelbrotter::FractalSpec;
using mandelbrotter::Mat2;
using mandelbrotter::ReferenceOrbit;

constexpr int    kBits = 128;
constexpr double kEps  = mandelbrotter::kBlaEpsilon;

/// The reference at -2: Z_0 = 0, Z_1 = -2, then 2 for ever, so every step doubles the delta four
/// times and the radii are easy to predict.
ReferenceOrbit tipOrbit(int maxIter)
{
    return {FractalSpec{}, BigComplex::fromComplex({-2.0, 0.0}, kBits), maxIter};
}

/// One perturbed step computed the slow, exact way: f(Z + d) + (C + dc) - (f(Z) + C), in big
/// numbers because the two terms cancel to a few digits in double.
Complex directDeltaStep(const FractalSpec& spec, Complex z, Complex d, Complex dc)
{
    constexpr int kExactBits = 256;
    const auto    step       = [&](const BigComplex& point) {
        BigComplex w = point;
        if (spec.family == FractalFamily::BURNING_SHIP)
        {
            w = point.absParts();
        }
        else if (spec.family == FractalFamily::TRICORN)
        {
            w = point.conj();
        }
        BigComplex result = w;
        for (int i = 1; i < spec.exponent; ++i)
        {
            result = result * w;
        }
        return result;
    };
    const BigComplex big   = BigComplex::fromComplex(z, kExactBits);
    const BigComplex moved = big + BigComplex::fromComplex(d, kExactBits);
    return (step(moved) - step(big)).approx() + (spec.julia ? Complex{} : dc);
}

TEST(BlaTable, Mat2ActsLikeComplexMultiplicationAndComposes)
{
    const Complex w{0.5, -1.25};
    const Complex v{-2.0, 0.75};
    EXPECT_EQ(Mat2::identity().apply(v), v);
    EXPECT_EQ(Mat2::scaling(w).apply(v), w * v);
    const Mat2 conj{1.0, 0.0, 0.0, -1.0};
    const Mat2 both = Mat2::scaling(w) * conj;  // conjugate first, then scale
    EXPECT_EQ(both.apply(v), (w * Complex{v.re, -v.im}));
    EXPECT_EQ((Mat2::scaling(w) + conj).apply(v), Mat2::scaling(w).apply(v) + conj.apply(v));
    EXPECT_DOUBLE_EQ(Mat2::identity().operatorNorm(), 1.0);
    EXPECT_DOUBLE_EQ(Mat2::scaling(w).operatorNorm(), std::hypot(w.re, w.im));
    EXPECT_DOUBLE_EQ((Mat2{3.0, 0.0, 0.0, -2.0}).operatorNorm(), 3.0);
    EXPECT_DOUBLE_EQ((Mat2{0.0, 0.0, 0.0, 0.0}).operatorNorm(), 0.0);
}

TEST(BlaTable, SingleStepLinearisesEveryFamilyWithinItsRadius)
{
    struct Case
    {
        std::string_view name;
        FractalSpec      spec;
    };
    const auto cases = std::to_array<Case>({
        {"mandelbrot", FractalSpec{}},
        {"cubic", FractalSpec{.exponent = 3}},
        {"quintic", FractalSpec{.exponent = 5}},
        {"burning ship", FractalSpec{.family = FractalFamily::BURNING_SHIP}},
        {"tricorn", FractalSpec{.family = FractalFamily::TRICORN}},
        {"julia", FractalSpec{.julia = true, .seed = {-0.8, 0.156}}},
    });
    for (const Case& c : cases)
    {
        const ReferenceOrbit ref(c.spec, BigComplex::fromComplex({-0.4, 0.3}, kBits), 10);
        for (const int index : {1, 2, 5})
        {
            const Bla     bla = mandelbrotter::singleStepBla(ref, index);
            const Complex z   = ref.points()[static_cast<std::size_t>(index)];
            EXPECT_EQ(bla.length, 1U) << c.name;
            EXPECT_GT(bla.radius, 0.0) << c.name;
            EXPECT_LE(bla.radius, 2.0 * kEps * std::hypot(z.re, z.im)) << c.name;
            // A delta just inside the radius, in an arbitrary direction, and a delta-c of the
            // same size: the linear step matches the direct one to the promised epsilon.
            const double  r = 0.9 * bla.radius;
            const Complex d{r * 0.6, r * 0.8};
            const Complex dc{-r * 0.28, r * 0.96};
            const Complex linear = bla.m.apply(d) + bla.n.apply(dc);
            const Complex direct = directDeltaStep(c.spec, z, d, dc);
            const double  scale  = std::max(std::hypot(direct.re, direct.im), 1e-300);
            EXPECT_NEAR(linear.re, direct.re, 4.0 * kEps * scale + 1e-20) << c.name << " " << index;
            EXPECT_NEAR(linear.im, direct.im, 4.0 * kEps * scale + 1e-20) << c.name << " " << index;
        }
    }
}

TEST(BlaTable, BurningShipRadiusStaysOnOneSideOfTheFolds)
{
    // A reference point close to the imaginary axis: the radius cannot exceed that distance.
    const FractalSpec    ship{.family = FractalFamily::BURNING_SHIP};
    const ReferenceOrbit ref(ship, BigComplex::fromComplex({1e-12, 0.5}, kBits), 1);
    const Bla            bla = mandelbrotter::singleStepBla(ref, 1);  // Z_1 = c itself
    EXPECT_LE(bla.radius, 1e-12);
    EXPECT_GT(bla.radius, 0.0);
}

TEST(BlaTable, MergeComposesMatricesAndShrinksTheRadius)
{
    const ReferenceOrbit ref = tipOrbit(10);
    const Bla            x   = mandelbrotter::singleStepBla(ref, 2);  // Z_2 = 2: derivative 4
    const Bla            y   = mandelbrotter::singleStepBla(ref, 3);
    EXPECT_EQ(x.m, Mat2::scaling({4.0, 0.0}));
    EXPECT_EQ(x.n, Mat2::identity());
    EXPECT_DOUBLE_EQ(x.radius, 4.0 * kEps);

    const Bla merged = mandelbrotter::mergeBla(x, y, 0.0);
    EXPECT_EQ(merged.length, 2U);
    EXPECT_EQ(merged.m, Mat2::scaling({16.0, 0.0}));
    EXPECT_EQ(merged.n, Mat2::scaling({5.0, 0.0}));  // 4 * I + I
    // The delta after x is four times bigger and must still be within y's radius.
    EXPECT_DOUBLE_EQ(merged.radius, kEps);
    // A delta-c bound eats into the slack, and can use it up entirely.
    EXPECT_LT(mandelbrotter::mergeBla(x, y, kEps).radius, kEps);
    EXPECT_DOUBLE_EQ(mandelbrotter::mergeBla(x, y, 10.0 * kEps).radius, 0.0);
    // Zero derivative: x's own radius is the only limit.
    const Bla flat{.m = Mat2{}, .n = Mat2::identity(), .radius = 0.5, .length = 1};
    EXPECT_DOUBLE_EQ(mandelbrotter::mergeBla(flat, y, 0.0).radius, 0.5);
}

TEST(BlaTable, LevelsCoverTheOrbitInPowersOfTwo)
{
    const BlaTable table(tipOrbit(1000), 0.0);
    EXPECT_EQ(table.levels(), 9);  // 1000 >> 9 == 1, 1000 >> 10 == 0
    std::size_t expected = 0;
    for (int j = 1; j <= 9; ++j)
    {
        expected += static_cast<std::size_t>(1000 >> j);
    }
    EXPECT_EQ(table.entryCount(), expected);

    EXPECT_EQ(BlaTable(tipOrbit(1), 0.0).levels(), 0);
    EXPECT_EQ(BlaTable(tipOrbit(0), 0.0).levels(), 0);
    EXPECT_EQ(BlaTable(tipOrbit(1), 0.0).longestValid(0, 0.0, 100), nullptr);
}

TEST(BlaTable, LongestValidPicksTheLongestAlignedJumpTheDeltaAllows)
{
    // From index 2 on the derivative is 4 per step, so an entry of length L has radius
    // 4 eps / 4^(L - 1): 2 -> eps, 4 -> eps / 16, 8 -> eps / 4096.
    const BlaTable table(tipOrbit(1000), 0.0);
    const auto     norm2 = [](double r) { return r * r; };

    const Bla* eight = table.longestValid(8, norm2(kEps / 8192), 1000);
    ASSERT_NE(eight, nullptr);
    EXPECT_EQ(eight->length, 8U);
    EXPECT_DOUBLE_EQ(eight->radius, kEps / 4096);

    const Bla* four = table.longestValid(8, norm2(kEps / 32), 1000);
    ASSERT_NE(four, nullptr);
    EXPECT_EQ(four->length, 4U);

    const Bla* two = table.longestValid(8, norm2(kEps / 2), 1000);
    ASSERT_NE(two, nullptr);
    EXPECT_EQ(two->length, 2U);

    EXPECT_EQ(table.longestValid(8, norm2(2.0 * kEps), 1000), nullptr);   // beyond every radius
    EXPECT_EQ(table.longestValid(3, norm2(kEps / 8192), 1000), nullptr);  // odd: nothing aligned
    EXPECT_EQ(table.longestValid(0, 0.0, 1000), nullptr);  // Z_0 = 0 cannot be linearised
    EXPECT_EQ(table.longestValid(-1, 0.0, 1000), nullptr);

    // The remaining iteration budget caps the length.
    const Bla* capped = table.longestValid(8, norm2(kEps / 8192), 5);
    ASSERT_NE(capped, nullptr);
    EXPECT_EQ(capped->length, 4U);
    EXPECT_EQ(table.longestValid(8, norm2(kEps / 8192), 1), nullptr);

    // Near the end of the orbit only the entries that fit exist.
    const Bla* tail = table.longestValid(992, 0.0, 1000);
    ASSERT_NE(tail, nullptr);
    EXPECT_EQ(tail->length, 8U);  // 992 + 8 = 1000 fits, 992 + 16 does not
}

}  // namespace
