#include "Mandelbrotter/perturbation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>

#include "Mandelbrotter/ReferenceOrbit.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/kernel.h"

namespace mandelbrotter
{

namespace
{

/// The reference point and the pixel's delta after the family's twist.
struct Twisted
{
    Complex w;
    Complex dw;
};

template <FractalFamily Family>
constexpr Twisted twist(Complex z, Complex d) noexcept
{
    if constexpr (Family == FractalFamily::BURNING_SHIP)
    {
        return {{std::abs(z.re), std::abs(z.im)}, {diffAbs(z.re, d.re), diffAbs(z.im, d.im)}};
    }
    else if constexpr (Family == FractalFamily::TRICORN)
    {
        return {{z.re, -z.im}, {d.re, -d.im}};
    }
    else
    {
        return {z, d};
    }
}

/// (w + d)^n - w^n without the cancellation of computing both powers: a^n - b^n = (a - b) * T with
/// T = sum over j of a^j b^(n-1-j), built up as T_m = a T_(m-1) + b^m.
constexpr Complex powerDelta(Complex w, Complex d, int n) noexcept
{
    const Complex a    = w + d;
    Complex       t    = {1.0, 0.0};
    Complex       wPow = {1.0, 0.0};
    for (int m = 1; m < n; ++m)
    {
        wPow = wPow * w;
        t    = t * a + wPow;
    }
    return d * t;
}

/// The n == 2 case of powerDelta: 2 w d + d^2.
constexpr Complex squareDelta(Complex w, Complex d) noexcept
{
    return d * (w + w + d);
}

/// |z|_1: the rebase test uses this norm because |z|^2 underflows long before z does (deltas at
/// zoom 1e300 are around 1e-303).
constexpr double normL1(Complex z) noexcept
{
    return std::abs(z.re) + std::abs(z.im);
}

struct RawResult
{
    int    steps;
    double normSquared;
    bool   escaped;
};

/// The hot loop, on a reference of at least two points. `Square` selects the n == 2 path at
/// compile time.
template <FractalFamily Family, bool Square>
RawResult run(std::span<const Complex> ref, Complex delta0, bool julia, int n, int maxIter) noexcept
{
    const Complex deltaC = julia ? Complex{} : delta0;  // the pixel's own c differs by this
    Complex       delta  = julia ? delta0 : Complex{};  // z_0 - Z_0
    const auto    last   = static_cast<int>(ref.size()) - 1;
    int           k      = 0;
    Complex       z      = ref[0] + delta;
    for (int steps = 0; steps < maxIter; ++steps)
    {
        const double r2 = z.normSquared();
        if (r2 > kBailoutRadiusSquared)
        {
            return {steps, r2, true};
        }
        if (k == last || normL1(z) < normL1(delta))
        {
            delta = z - ref[0];
            k     = 0;
        }
        const auto [w, dw] = twist<Family>(ref[static_cast<std::size_t>(k)], delta);
        delta              = (Square ? squareDelta(w, dw) : powerDelta(w, dw, n)) + deltaC;
        ++k;
        z = ref[static_cast<std::size_t>(k)] + delta;
    }
    const double r2 = z.normSquared();
    return {maxIter, r2, r2 > kBailoutRadiusSquared};
}

template <FractalFamily Family>
RawResult runFamily(std::span<const Complex> ref, Complex delta0, bool julia, int n,
                    int maxIter) noexcept
{
    return n == 2 ? run<Family, true>(ref, delta0, julia, n, maxIter)
                  : run<Family, false>(ref, delta0, julia, n, maxIter);
}

RawResult runAny(const FractalSpec& spec, std::span<const Complex> ref, Complex delta0, int n,
                 int maxIter) noexcept
{
    switch (spec.family)
    {
        case FractalFamily::MANDELBROT:
            return runFamily<FractalFamily::MANDELBROT>(ref, delta0, spec.julia, n, maxIter);
        case FractalFamily::BURNING_SHIP:
            return runFamily<FractalFamily::BURNING_SHIP>(ref, delta0, spec.julia, n, maxIter);
        case FractalFamily::TRICORN:
            return runFamily<FractalFamily::TRICORN>(ref, delta0, spec.julia, n, maxIter);
    }
    return {maxIter, 0.0, false};
}

}  // namespace

IterationResult iteratePerturbed(const ReferenceOrbit& reference, Complex delta0,
                                 int maxIter) noexcept
{
    const FractalSpec& spec = reference.spec();
    const int          n    = clampExponent(spec.exponent);
    maxIter                 = std::max(maxIter, 0);
    if (reference.length() < 2)
    {
        maxIter = 0;  // a lone Z_0 gives nothing to step along
    }
    const RawResult raw = runAny(spec, reference.points(), delta0, n, maxIter);
    return smoothIterationResult(raw.steps, raw.normSquared, raw.escaped, n);
}

}  // namespace mandelbrotter
