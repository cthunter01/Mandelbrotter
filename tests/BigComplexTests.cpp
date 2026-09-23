#include "Mandelbrotter/BigComplex.h"

#include <gtest/gtest.h>

#include "Mandelbrotter/BigFixed.h"
#include "Mandelbrotter/geometry.h"

namespace
{

using mandelbrotter::BigComplex;
using mandelbrotter::BigFixed;
using mandelbrotter::Complex;

constexpr int kBits = 128;

BigComplex big(double re, double im, int fractionBits = kBits)
{
    return BigComplex::fromComplex({re, im}, fractionBits);
}

TEST(BigComplex, DefaultIsZero)
{
    const BigComplex zero;
    EXPECT_TRUE(zero.re.isZero());
    EXPECT_TRUE(zero.im.isZero());
    EXPECT_EQ(zero.fractionBits(), BigFixed::kMinFractionBits);
    EXPECT_EQ(BigComplex{kBits}.fractionBits(), kBits);
}

TEST(BigComplex, ComplexRoundTrip)
{
    const Complex c{-0.7436438870371587, 0.1318259042053119};
    EXPECT_EQ(BigComplex::fromComplex(c, kBits).approx(), c);
    EXPECT_EQ(big(1.5, -2.25).approx(), (Complex{1.5, -2.25}));
}

TEST(BigComplex, FromDecimalParsesBothParts)
{
    const auto z = BigComplex::fromDecimal("-0.75", "0.125", kBits);
    ASSERT_TRUE(z.has_value());
    EXPECT_EQ(*z, big(-0.75, 0.125));
    EXPECT_EQ(z->fractionBits(), kBits);
    EXPECT_FALSE(BigComplex::fromDecimal("x", "0", kBits).has_value());
    EXPECT_FALSE(BigComplex::fromDecimal("0", "", kBits).has_value());
}

TEST(BigComplex, ScaledMultipliesBothParts)
{
    const BigComplex z = big(1.5, -0.5).scaled(0.5, 256);
    EXPECT_EQ(z, big(0.75, -0.25));
    EXPECT_EQ(z.fractionBits(), 256);
    EXPECT_EQ(big(1.0, 1.0).scaled(0.0, kBits), big(0.0, 0.0));
    EXPECT_EQ(big(-2.0, 4.0).scaled(-0.25, kBits), big(0.5, -1.0));
}

TEST(BigComplex, WithFractionBitsAppliesToBothParts)
{
    const BigComplex z = big(0.5, -0.25).withFractionBits(256);
    EXPECT_EQ(z.re.fractionBits(), 256);
    EXPECT_EQ(z.im.fractionBits(), 256);
    EXPECT_EQ(z, big(0.5, -0.25));
}

TEST(BigComplex, ConjugateAndAbsParts)
{
    const BigComplex z = big(-1.5, 2.5);
    EXPECT_EQ(z.conj(), big(-1.5, -2.5));
    EXPECT_EQ(z.absParts(), big(1.5, 2.5));
    EXPECT_EQ(big(1.5, -2.5).absParts(), big(1.5, 2.5));
}

TEST(BigComplex, Arithmetic)
{
    const BigComplex a = big(1.5, -0.5);
    const BigComplex b = big(-0.25, 2.0);
    EXPECT_EQ(a + b, big(1.25, 1.5));
    EXPECT_EQ(a - b, big(1.75, -2.5));
    // (1.5 - 0.5i)(-0.25 + 2i) = -0.375 + 3i + 0.125i + 1 = 0.625 + 3.125i
    EXPECT_EQ(a * b, big(0.625, 3.125));
    EXPECT_EQ(b * a, big(0.625, 3.125));
}

TEST(BigComplex, SquaredMatchesTheProduct)
{
    for (const Complex c : {Complex{1.5, -0.5}, Complex{-0.75, 0.125}, Complex{0.0, 2.0}})
    {
        const BigComplex z = BigComplex::fromComplex(c, kBits);
        EXPECT_EQ(z.squared(), z * z);
        EXPECT_EQ(z.squared().approx(), c * c);
    }
}

TEST(BigComplex, NormSquaredApprox)
{
    EXPECT_DOUBLE_EQ(big(3.0, 4.0).normSquaredApprox(), 25.0);
    EXPECT_DOUBLE_EQ(big(-0.5, 0.25).normSquaredApprox(), 0.3125);
}

TEST(BigComplex, OrbitAgreesWithDoubleIterationWhileItStaysBounded)
{
    const Complex    c{-0.5, 0.5};
    const BigComplex bigC = BigComplex::fromComplex(c, kBits);
    Complex          z{};
    BigComplex       bigZ{kBits};
    for (int k = 0; k < 40; ++k)
    {
        z                    = z * z + c;
        bigZ                 = bigZ.squared() + bigC;
        const Complex approx = bigZ.approx();
        EXPECT_NEAR(approx.re, z.re, 1e-9) << "iteration " << k;
        EXPECT_NEAR(approx.im, z.im, 1e-9) << "iteration " << k;
        EXPECT_LT(bigZ.normSquaredApprox(), 4.0);
    }
}

}  // namespace
