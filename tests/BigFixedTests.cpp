#include "Mandelbrotter/BigFixed.h"

#include <cmath>
#include <compare>
#include <limits>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace
{

using mandelbrotter::BigFixed;

constexpr int kBits = 256;

BigFixed dec(std::string_view text, int fractionBits = kBits)
{
    const auto value = BigFixed::fromDecimal(text, fractionBits);
    EXPECT_TRUE(value.has_value()) << text;
    return value.value_or(BigFixed{fractionBits});
}

BigFixed dbl(double value, int fractionBits = kBits)
{
    return BigFixed::fromDouble(value, fractionBits);
}

TEST(BigFixed, DefaultIsZeroAtMinimumPrecision)
{
    const BigFixed zero;
    EXPECT_TRUE(zero.isZero());
    EXPECT_FALSE(zero.isNegative());
    EXPECT_EQ(zero.toDouble(), 0.0);
    EXPECT_EQ(zero.fractionBits(), BigFixed::kMinFractionBits);
}

TEST(BigFixed, FractionBitsRoundUpToWholeLimbsAndAreClamped)
{
    EXPECT_EQ(BigFixed{65}.fractionBits(), 96);
    EXPECT_EQ(BigFixed{96}.fractionBits(), 96);
    EXPECT_EQ(BigFixed{1}.fractionBits(), BigFixed::kMinFractionBits);
    EXPECT_EQ(BigFixed{-5}.fractionBits(), BigFixed::kMinFractionBits);
    EXPECT_EQ(BigFixed{1 << 20}.fractionBits(), BigFixed::kMaxFractionBits);
}

TEST(BigFixed, DoubleRoundTripsExactlyWhenTheBitsFit)
{
    for (const double value : {0.0, 1.0, -1.0, 0.5, -0.75, 3.15625, 1e-10, -123456.789, 255.999,
                               1e20, -2.5e-20, std::ldexp(1.0, 90), -std::ldexp(1.0, 90)})
    {
        EXPECT_EQ(dbl(value, 128).toDouble(), value) << value;
    }
}

TEST(BigFixed, FromDoubleTruncatesTowardZero)
{
    EXPECT_TRUE(dbl(std::ldexp(1.0, -70), 64).isZero());
    EXPECT_TRUE(dbl(-std::ldexp(1.0, -70), 64).isZero());
    EXPECT_FALSE(dbl(std::ldexp(1.0, -70), 128).isZero());
    // 1 + 2^-50 fits in 64 fraction bits, so nothing is lost.
    EXPECT_EQ(dbl(1.0 + std::ldexp(1.0, -50), 64).toDouble(), 1.0 + std::ldexp(1.0, -50));
}

TEST(BigFixed, NonFiniteBecomesZero)
{
    EXPECT_TRUE(dbl(std::numeric_limits<double>::quiet_NaN()).isZero());
    EXPECT_TRUE(dbl(std::numeric_limits<double>::infinity()).isZero());
    EXPECT_TRUE(dbl(-std::numeric_limits<double>::infinity()).isZero());
}

TEST(BigFixed, AdditionAndSubtractionAreExact)
{
    EXPECT_EQ(dbl(1.5) + dbl(-0.25), dbl(1.25));
    EXPECT_EQ(dbl(-0.25) + dbl(1.5), dbl(1.25));
    EXPECT_EQ(dbl(1.5) - dbl(2.0), dbl(-0.5));
    EXPECT_EQ(dbl(-1.5) - dbl(2.0), dbl(-3.5));
    EXPECT_EQ((dbl(0.1) + dbl(0.2) - dbl(0.2)), dbl(0.1));
    EXPECT_EQ((dbl(1e20) + dbl(1e-20)).toDouble(), 1e20);
    EXPECT_EQ((dbl(1e20) + dbl(1e-20) - dbl(1e20)), dbl(1e-20));
}

TEST(BigFixed, NegationAndAbs)
{
    EXPECT_EQ(-dbl(0.75), dbl(-0.75));
    EXPECT_EQ(-dbl(-0.75), dbl(0.75));
    EXPECT_EQ(dbl(-0.75).abs(), dbl(0.75));
    EXPECT_EQ(dbl(0.75).abs(), dbl(0.75));
    EXPECT_TRUE((-BigFixed{}).isZero());
    EXPECT_TRUE(dbl(-0.75).isNegative());
    EXPECT_FALSE(dbl(0.75).isNegative());
}

TEST(BigFixed, MultiplicationOfRepresentableValuesIsExact)
{
    EXPECT_EQ(dbl(1.5) * dbl(-0.25), dbl(-0.375));
    EXPECT_EQ(dbl(-1.5) * dbl(-0.25), dbl(0.375));
    EXPECT_EQ(dbl(1e10) * dbl(1e10), dbl(1e20));
    EXPECT_EQ(dbl(123456789.0) * dbl(987654321.0), dec("121932631112635269"));
    EXPECT_EQ(dbl(std::ldexp(1.0, 40)) * dbl(std::ldexp(1.0, 40)), dbl(std::ldexp(1.0, 80)));
    EXPECT_EQ(dbl(std::ldexp(1.0, -40)) * dbl(std::ldexp(1.0, 40)), dbl(1.0));
    EXPECT_EQ(dbl(3.0 * std::ldexp(1.0, -40)) * dbl(5.0 * std::ldexp(1.0, -30)),
              dbl(15.0 * std::ldexp(1.0, -70)));
}

TEST(BigFixed, MultiplicationBySmallIntegersMatchesRepeatedAddition)
{
    const BigFixed x   = dec("0.1234567890123456789012345678901234567890123456789");
    BigFixed       sum = x;
    for (int i = 1; i < 7; ++i)
    {
        sum += x;
    }
    EXPECT_EQ(x * dbl(7.0), sum);
    EXPECT_EQ(dbl(7.0) * x, sum);
    EXPECT_EQ(x * dbl(-7.0), -sum);
}

TEST(BigFixed, MultiplicationTruncatesTowardZero)
{
    const BigFixed tiny = dbl(std::ldexp(1.0, -40), 64);
    EXPECT_TRUE((tiny * tiny).isZero());
    EXPECT_TRUE(((-tiny) * tiny).isZero());
    EXPECT_FALSE(((-tiny) * tiny).isNegative());
    // The decimal 0.1 is stored slightly below 0.1, so 0.1 * 10 lands just under 1.
    const BigFixed nearlyOne = dec("0.1") * dbl(10.0);
    EXPECT_LE(nearlyOne, dbl(1.0));
    EXPECT_GT(nearlyOne, dbl(1.0) - dbl(std::ldexp(1.0, -200)));
}

TEST(BigFixed, ProductsAreConsistentWithinRoundingError)
{
    const BigFixed a   = dec("0.7071067811865475244008443621048490392848359376884740365883398689");
    const BigFixed b   = dec("-0.3333333333333333333333333333333333333333333333333333333333333333");
    const BigFixed c   = dec("2.7182818284590452353602874713526624977572470936999595749669676277");
    const BigFixed ulp = dbl(std::ldexp(1.0, -kBits));
    const BigFixed few = ulp * dbl(8.0);
    EXPECT_LT(((a * b) * c - a * (b * c)).abs(), few);
    EXPECT_LT(((a + b) * (a + b) - (a * a + a * b + a * b + b * b)).abs(), few);
    EXPECT_EQ(a * b, b * a);
}

TEST(BigFixed, ComparisonIsNumeric)
{
    EXPECT_LT(dbl(-1.0), dbl(-0.5));
    EXPECT_LT(dbl(-0.5), BigFixed{});
    EXPECT_LT(BigFixed{}, dbl(0.5));
    EXPECT_LT(dbl(0.5), dbl(1.0));
    EXPECT_GT(dbl(1.0), dbl(-1.0));
    EXPECT_EQ(dbl(0.75), dbl(0.75));
    EXPECT_NE(dbl(0.75), dbl(0.7500001));
    EXPECT_LE(dbl(2.0), dbl(2.0));
    EXPECT_GE(dbl(-2.0), dbl(-2.0));
}

TEST(BigFixed, ComparisonIgnoresPrecision)
{
    const BigFixed coarse = dbl(0.75, 64);
    const BigFixed fine   = dbl(0.75, 256);
    EXPECT_EQ(coarse, fine);
    EXPECT_EQ(fine, coarse);
    EXPECT_EQ(coarse <=> fine, std::strong_ordering::equal);
    const BigFixed tiny  = dec("1e-60", 256);  // ~2^-199: only representable at 256 bits
    const BigFixed above = fine + tiny;
    EXPECT_LT(coarse, above);
    EXPECT_GT(above, coarse);
    EXPECT_LT(-above, -coarse);
    EXPECT_GT(-coarse, -above);
    EXPECT_NE(coarse, above);
}

TEST(BigFixed, PrecisionFollowsTheFinerOperand)
{
    EXPECT_EQ((dbl(1.0, 64) + dbl(1.0, 256)).fractionBits(), 256);
    EXPECT_EQ((dbl(1.0, 256) + dbl(1.0, 64)).fractionBits(), 256);
    EXPECT_EQ((dbl(1.0, 64) - dbl(1.0, 256)).fractionBits(), 256);
    EXPECT_EQ((dbl(1.0, 64) * dbl(1.0, 256)).fractionBits(), 256);
    EXPECT_EQ((dbl(1.0, 256) * dbl(1.0, 64)).fractionBits(), 256);
    EXPECT_EQ((dbl(0.5, 64) * dbl(0.5, 256)), dbl(0.25));
}

TEST(BigFixed, WithFractionBitsExtendsExactlyAndReducesTowardMinusInfinity)
{
    const BigFixed x = dec("0.1");
    EXPECT_EQ(x.withFractionBits(512), x);
    EXPECT_EQ(x.withFractionBits(512).fractionBits(), 512);
    const BigFixed reduced = x.withFractionBits(64);
    EXPECT_EQ(reduced.fractionBits(), 64);
    EXPECT_LE(reduced, x);
    EXPECT_LT(x - reduced, dbl(std::ldexp(1.0, -64)));
    const BigFixed reducedNegative = (-x).withFractionBits(64);
    EXPECT_LE(reducedNegative, -x);
    EXPECT_LT(-x - reducedNegative, dbl(std::ldexp(1.0, -64)));
}

TEST(BigFixed, ParsesEveryDecimalForm)
{
    EXPECT_EQ(dec("1"), dbl(1.0));
    EXPECT_EQ(dec("+1"), dbl(1.0));
    EXPECT_EQ(dec("-1"), dbl(-1.0));
    EXPECT_EQ(dec("1."), dbl(1.0));
    EXPECT_EQ(dec(".5"), dbl(0.5));
    EXPECT_EQ(dec("-.5"), dbl(-0.5));
    EXPECT_EQ(dec("0000.5000"), dbl(0.5));
    EXPECT_EQ(dec("1.5e3"), dbl(1500.0));
    EXPECT_EQ(dec("1.5E+3"), dbl(1500.0));
    EXPECT_EQ(dec("15e-1"), dbl(1.5));
    EXPECT_EQ(dec("1e0"), dbl(1.0));
    EXPECT_DOUBLE_EQ(dec("-7.25e-3").toDouble(), -0.00725);
    EXPECT_EQ(dec("0"), BigFixed{});
    EXPECT_EQ(dec("-0"), BigFixed{});
    EXPECT_EQ(dec("-0.0"), BigFixed{});
    EXPECT_DOUBLE_EQ(dec("0.1").toDouble(), 0.1);
    EXPECT_DOUBLE_EQ(dec("-0.7436438870371587").toDouble(), -0.7436438870371587);
}

TEST(BigFixed, RejectsMalformedDecimals)
{
    for (const std::string_view text : {"", "+", "-", ".", "e5", "1e", "1e+", "1.2.3", " 1", "1 ",
                                        "1,5", "abc", "0x10", "1e999999", "--1", "1e5.5", "1-2"})
    {
        EXPECT_FALSE(BigFixed::fromDecimal(text, kBits).has_value()) << "'" << text << "'";
    }
}

TEST(BigFixed, RejectsValuesOutsideTheIntegerRange)
{
    // 2^95 = 39614081257132168796771975168 is the first magnitude that does not fit.
    EXPECT_TRUE(BigFixed::fromDecimal("39614081257132168796771975167", kBits).has_value());
    EXPECT_FALSE(BigFixed::fromDecimal("39614081257132168796771975168", kBits).has_value());
    EXPECT_FALSE(BigFixed::fromDecimal("1e30", kBits).has_value());
    EXPECT_TRUE(BigFixed::fromDecimal("-39614081257132168796771975167", kBits).has_value());
    EXPECT_FALSE(BigFixed::fromDecimal("-1e29", kBits).has_value());
}

TEST(BigFixed, DecimalOutputRoundsHalfUp)
{
    EXPECT_EQ(dec("-0.75").toDecimal(4), "-0.7500");
    EXPECT_EQ(dec("12345.678").toDecimal(3), "12345.678");
    EXPECT_EQ(dec("12345.678").toDecimal(6), "12345.678000");
    EXPECT_EQ(dec("12345.678").toDecimal(1), "12345.7");
    EXPECT_EQ(dbl(2.0 / 3.0).toDecimal(4), "0.6667");
    EXPECT_EQ(dec("0.99996").toDecimal(4), "1.0000");
    EXPECT_EQ(dec("-0.99996").toDecimal(4), "-1.0000");
    EXPECT_EQ(dec("9.99996").toDecimal(4), "10.0000");
    EXPECT_EQ(dec("-0.00004").toDecimal(4), "0.0000");
    EXPECT_EQ(dec("-0.00006").toDecimal(4), "-0.0001");
    EXPECT_EQ(dec("2.5").toDecimal(0), "3");
    EXPECT_EQ(dec("-2.4").toDecimal(0), "-2");
    EXPECT_EQ(dec("0.4").toDecimal(-3), "0");
    EXPECT_EQ(BigFixed{}.toDecimal(2), "0.00");
    EXPECT_EQ(dec("1e20").toDecimal(1), "100000000000000000000.0");
}

TEST(BigFixed, LongDecimalsRoundTrip)
{
    std::string fraction;
    for (int i = 0; i < 120; ++i)
    {
        fraction.push_back(static_cast<char>('0' + (i * 7 + 3) % 10));
    }
    const std::string text = "-123456789012345678901234567." + fraction;
    EXPECT_EQ(dec(text, 512).toDecimal(120), text);
    EXPECT_EQ(dec("0." + fraction, 512).toDecimal(120), "0." + fraction);
    // Printed with more digits than it has, the rest is zeros.
    EXPECT_EQ(dec("0.5", 512).toDecimal(40), "0.5000000000000000000000000000000000000000");
}

}  // namespace
