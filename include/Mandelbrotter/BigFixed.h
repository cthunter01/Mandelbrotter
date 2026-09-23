#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mandelbrotter
{

/// A signed fixed-point number with a precision chosen at run time: kIntegerBits before the binary
/// point and a multiple of 32 bits after it. Two's complement over 32-bit limbs (least significant
/// first), so it needs nothing beyond 64-bit arithmetic and behaves identically on every compiler.
///
/// Built for perturbation rendering, where the reference orbit stays bounded (|z| below the bailout
/// radius, one z^n step below 2^64 for n <= 8) but needs far more digits than a double. Staying in
/// range is the caller's job: arithmetic wraps silently outside +-2^(kIntegerBits - 1).
/// + and - are exact; * truncates the exact product toward zero. Operands of different precision
/// are aligned to the finer one, which the result keeps.
class BigFixed
{
public:
    static constexpr int kLimbBits        = 32;
    static constexpr int kIntegerLimbs    = 3;
    static constexpr int kIntegerBits     = kIntegerLimbs * kLimbBits;
    static constexpr int kMinFractionBits = 64;
    static constexpr int kMaxFractionBits = 1 << 16;

    /// Zero with `fractionBits` after the point, rounded up to whole limbs and clamped to
    /// [kMinFractionBits, kMaxFractionBits].
    explicit BigFixed(int fractionBits = kMinFractionBits);

    /// `value` truncated toward zero to the precision (exact whenever its bits fit). Non-finite
    /// values give zero.
    [[nodiscard]] static BigFixed fromDouble(double value, int fractionBits);
    /// Parses "[+-]digits[.digits][(e|E)[+-]digits]" with at least one digit ("1.", ".5" and
    /// "-7.25e-3" are all fine), rounding to the nearest representable value (ties away from
    /// zero). nullopt for anything else and for values outside the integer range.
    [[nodiscard]] static std::optional<BigFixed> fromDecimal(std::string_view text,
                                                             int              fractionBits);

    [[nodiscard]] int fractionBits() const noexcept;
    /// The nearest double, to within an ulp.
    [[nodiscard]] double toDouble() const;
    /// Plain decimal notation with exactly `fractionDigits` digits after the point, rounded to
    /// nearest (half up): "-0.7500". fractionDigits <= 0 gives the rounded integer alone.
    [[nodiscard]] std::string toDecimal(int fractionDigits) const;
    /// The same value at another precision: extending is exact, reducing rounds toward -infinity.
    [[nodiscard]] BigFixed withFractionBits(int fractionBits) const;

    [[nodiscard]] bool     isZero() const noexcept;
    [[nodiscard]] bool     isNegative() const noexcept;
    [[nodiscard]] BigFixed abs() const;

    [[nodiscard]] BigFixed operator-() const;
    BigFixed&              operator+=(const BigFixed& other);
    BigFixed&              operator-=(const BigFixed& other);
    BigFixed&              operator*=(const BigFixed& other);

    friend BigFixed operator+(BigFixed a, const BigFixed& b)
    {
        a += b;
        return a;
    }
    friend BigFixed operator-(BigFixed a, const BigFixed& b)
    {
        a -= b;
        return a;
    }
    friend BigFixed operator*(BigFixed a, const BigFixed& b)
    {
        a *= b;
        return a;
    }

    /// Numeric comparisons: the precisions need not match.
    friend bool                 operator==(const BigFixed& a, const BigFixed& b) noexcept;
    friend std::strong_ordering operator<=>(const BigFixed& a, const BigFixed& b) noexcept;

private:
    using Limb = std::uint32_t;

    explicit BigFixed(std::vector<Limb> limbs) noexcept;
    [[nodiscard]] int fractionLimbs() const noexcept;

    /// Fraction limbs first, then the kIntegerLimbs integer limbs; the top bit is the sign.
    std::vector<Limb> m_limbs;
};

}  // namespace mandelbrotter
