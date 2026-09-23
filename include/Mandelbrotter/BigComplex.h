#pragma once

#include <optional>
#include <string_view>
#include <utility>

#include "Mandelbrotter/BigFixed.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter
{

/// A complex number over BigFixed: the centre of a deep view and the reference orbit computed from
/// it. Both parts are kept at the same precision.
struct BigComplex
{
    BigFixed re;
    BigFixed im;

    explicit BigComplex(int fractionBits = BigFixed::kMinFractionBits)
      : re(fractionBits), im(fractionBits)
    {
    }
    BigComplex(BigFixed real, BigFixed imaginary)
      : re(std::move(real)), im(std::move(imaginary)) { }
    /// From doubles at the minimum precision: what `{re, im}` in an initializer means.
    BigComplex(double real, double imaginary)
      : re(BigFixed::fromDouble(real, BigFixed::kMinFractionBits)),
        im(BigFixed::fromDouble(imaginary, BigFixed::kMinFractionBits))
    {
    }

    [[nodiscard]] static BigComplex fromComplex(Complex c, int fractionBits);
    /// Both parts from decimal strings (BigFixed::fromDecimal); nullopt if either is malformed.
    [[nodiscard]] static std::optional<BigComplex> fromDecimal(std::string_view real,
                                                               std::string_view imaginary,
                                                               int              fractionBits);
    /// Both parts as the nearest doubles.
    [[nodiscard]] Complex    approx() const;
    [[nodiscard]] int        fractionBits() const noexcept { return re.fractionBits(); }
    [[nodiscard]] BigComplex withFractionBits(int fractionBits) const;
    /// Both parts times `factor` (truncated like BigFixed's *), at `fractionBits`.
    [[nodiscard]] BigComplex scaled(double factor, int fractionBits) const;

    [[nodiscard]] BigComplex conj() const;
    /// (|re|, |im|): the Burning Ship's fold.
    [[nodiscard]] BigComplex absParts() const;
    /// z * z with two multiplications instead of four.
    [[nodiscard]] BigComplex squared() const;
    /// |z|^2 as a double: plenty for an escape test.
    [[nodiscard]] double normSquaredApprox() const;

    bool operator==(const BigComplex&) const = default;

    friend BigComplex operator+(const BigComplex& a, const BigComplex& b)
    {
        return {a.re + b.re, a.im + b.im};
    }
    friend BigComplex operator-(const BigComplex& a, const BigComplex& b)
    {
        return {a.re - b.re, a.im - b.im};
    }
    friend BigComplex operator*(const BigComplex& a, const BigComplex& b);
};

}  // namespace mandelbrotter
