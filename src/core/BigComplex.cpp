#include "Mandelbrotter/BigComplex.h"

#include <optional>
#include <string_view>

#include "Mandelbrotter/BigFixed.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter
{

BigComplex BigComplex::fromComplex(Complex c, int fractionBits)
{
    return {BigFixed::fromDouble(c.re, fractionBits), BigFixed::fromDouble(c.im, fractionBits)};
}

std::optional<BigComplex> BigComplex::fromDecimal(std::string_view real, std::string_view imaginary,
                                                  int fractionBits)
{
    const auto realPart      = BigFixed::fromDecimal(real, fractionBits);
    const auto imaginaryPart = BigFixed::fromDecimal(imaginary, fractionBits);
    if (!realPart || !imaginaryPart)
    {
        return std::nullopt;
    }
    return BigComplex{*realPart, *imaginaryPart};
}

Complex BigComplex::approx() const
{
    return {re.toDouble(), im.toDouble()};
}

BigComplex BigComplex::withFractionBits(int fractionBits) const
{
    return {re.withFractionBits(fractionBits), im.withFractionBits(fractionBits)};
}

BigComplex BigComplex::scaled(double factor, int fractionBits) const
{
    const BigFixed scale = BigFixed::fromDouble(factor, fractionBits);
    return {re.withFractionBits(fractionBits) * scale, im.withFractionBits(fractionBits) * scale};
}

BigComplex BigComplex::conj() const
{
    return {re, -im};
}

BigComplex BigComplex::absParts() const
{
    return {re.abs(), im.abs()};
}

BigComplex BigComplex::squared() const
{
    // re^2 - im^2 = (re + im)(re - im); 2 re im = cross + cross.
    const BigFixed cross = re * im;
    return {(re + im) * (re - im), cross + cross};
}

double BigComplex::normSquaredApprox() const
{
    return approx().normSquared();
}

BigComplex operator*(const BigComplex& a, const BigComplex& b)
{
    return {a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re};
}

}  // namespace mandelbrotter
