#include "Mandelbrotter/kernel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter
{

namespace
{

/// The per-family twist applied to z before raising it to the power.
template <FractalFamily Family>
constexpr Complex transform(Complex z) noexcept
{
    if constexpr (Family == FractalFamily::BurningShip)
    {
        return {std::abs(z.re), std::abs(z.im)};
    }
    else if constexpr (Family == FractalFamily::Tricorn)
    {
        return {z.re, -z.im};
    }
    else
    {
        return z;
    }
}

constexpr Complex integerPower(Complex z, int n) noexcept
{
    Complex result = z;
    for (int i = 1; i < n; ++i)
    {
        result = result * z;
    }
    return result;
}

constexpr Complex square(Complex w, Complex c) noexcept
{
    return {w.re * w.re - w.im * w.im + c.re, 2.0 * w.re * w.im + c.im};
}

template <FractalFamily Family>
constexpr Complex step(Complex z, Complex c, int n) noexcept
{
    const Complex w = transform<Family>(z);
    return n == 2 ? square(w, c) : integerPower(w, n) + c;
}

Complex stepAny(const FractalSpec& spec, Complex z, Complex c) noexcept
{
    const int n = clampExponent(spec.exponent);
    switch (spec.family)
    {
        case FractalFamily::Mandelbrot:
            return step<FractalFamily::Mandelbrot>(z, c, n);
        case FractalFamily::BurningShip:
            return step<FractalFamily::BurningShip>(z, c, n);
        case FractalFamily::Tricorn:
            return step<FractalFamily::Tricorn>(z, c, n);
    }
    return z;
}

struct RawResult
{
    int    steps;
    double normSquared;
    bool   escaped;
};

/// The hot loop. `Square` selects the n == 2 fast path at compile time so the loop body has no
/// branch on n.
template <FractalFamily Family, bool Square>
RawResult run(Complex z, Complex c, int n, int maxIter) noexcept
{
    for (int k = 0; k < maxIter; ++k)
    {
        const double r2 = z.normSquared();
        if (r2 > kBailoutRadiusSquared)
        {
            return {k, r2, true};
        }
        const Complex w = transform<Family>(z);
        if constexpr (Square)
        {
            z = square(w, c);
        }
        else
        {
            z = integerPower(w, n) + c;
        }
    }
    const double r2 = z.normSquared();
    return {maxIter, r2, r2 > kBailoutRadiusSquared};
}

template <FractalFamily Family>
RawResult runFamily(Complex z0, Complex c, int n, int maxIter) noexcept
{
    return n == 2 ? run<Family, true>(z0, c, n, maxIter) : run<Family, false>(z0, c, n, maxIter);
}

RawResult runAny(const FractalSpec& spec, Complex z0, Complex c, int n, int maxIter) noexcept
{
    switch (spec.family)
    {
        case FractalFamily::Mandelbrot:
            return runFamily<FractalFamily::Mandelbrot>(z0, c, n, maxIter);
        case FractalFamily::BurningShip:
            return runFamily<FractalFamily::BurningShip>(z0, c, n, maxIter);
        case FractalFamily::Tricorn:
            return runFamily<FractalFamily::Tricorn>(z0, c, n, maxIter);
    }
    return {maxIter, 0.0, false};
}

}  // namespace

OrbitStart orbitStart(const FractalSpec& spec, Complex p) noexcept
{
    if (spec.julia)
    {
        return {.z0 = p, .c = spec.seed};
    }
    return {.z0 = {0.0, 0.0}, .c = p};
}

IterationResult iterate(const FractalSpec& spec, Complex z0, Complex c, int maxIter) noexcept
{
    maxIter             = std::max(maxIter, 0);
    const int       n   = clampExponent(spec.exponent);
    const RawResult raw = runAny(spec, z0, c, n, maxIter);
    if (!raw.escaped)
    {
        return {.smoothIter = static_cast<double>(maxIter), .interior = true};
    }
    // |z| lies between B and roughly B^n at escape. log(log|z| / log B) / log n maps that range
    // onto [0, 1), which makes the count continuous across integer steps.
    const double logModulus = 0.5 * std::log(raw.normSquared);
    const double fraction =
        std::log(logModulus / std::log(kBailoutRadius)) / std::log(static_cast<double>(n));
    return {.smoothIter = std::max(0.0, raw.steps - fraction), .interior = false};
}

IterationResult iteratePixel(const FractalSpec& spec, Complex p, int maxIter) noexcept
{
    const OrbitStart start = orbitStart(spec, p);
    return iterate(spec, start.z0, start.c, maxIter);
}

std::vector<Complex> orbit(const FractalSpec& spec, Complex p, int maxPoints)
{
    std::vector<Complex> points;
    if (maxPoints <= 0)
    {
        return points;
    }
    points.reserve(static_cast<std::size_t>(std::min(maxPoints, 1024)));
    const OrbitStart start = orbitStart(spec, p);
    Complex          z     = start.z0;
    for (int k = 0; k < maxPoints; ++k)
    {
        points.push_back(z);
        if (z.normSquared() > kBailoutRadiusSquared)
        {
            break;
        }
        z = stepAny(spec, z, start.c);
    }
    return points;
}

}  // namespace mandelbrotter
