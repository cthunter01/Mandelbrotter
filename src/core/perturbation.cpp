#include "Mandelbrotter/perturbation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>

#include "Mandelbrotter/BlaTable.h"
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

/// What run() needs beyond the reference: the pixel, the limits and the optional jump table.
struct Task
{
    Complex         delta0;
    bool            julia;
    int             n;
    int             maxIter;
    const BlaTable* bla;
};

/// The hot loop, on a reference of at least two points. `Square` selects the n == 2 path at
/// compile time. `skipped` counts the steps covered by BLA jumps.
template <FractalFamily Family, bool Square>
RawResult run(std::span<const Complex> ref, const Task& task, int& skipped) noexcept
{
    const Complex deltaC = task.julia ? Complex{} : task.delta0;  // the pixel's c differs by this
    Complex       delta  = task.julia ? task.delta0 : Complex{};  // z_0 - Z_0
    const auto    last   = static_cast<int>(ref.size()) - 1;
    int           k      = 0;
    int           steps  = 0;
    Complex       z      = ref[0] + delta;
    while (steps < task.maxIter)
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
        if (task.bla != nullptr)
        {
            if (const Bla* jump =
                    task.bla->longestValid(k, delta.normSquared(), task.maxIter - steps))
            {
                delta = jump->m.apply(delta) + jump->n.apply(deltaC);
                k += static_cast<int>(jump->length);
                steps += static_cast<int>(jump->length);
                skipped += static_cast<int>(jump->length);
                z = ref[static_cast<std::size_t>(k)] + delta;
                continue;
            }
        }
        const auto [w, dw] = twist<Family>(ref[static_cast<std::size_t>(k)], delta);
        delta              = (Square ? squareDelta(w, dw) : powerDelta(w, dw, task.n)) + deltaC;
        ++k;
        ++steps;
        z = ref[static_cast<std::size_t>(k)] + delta;
    }
    const double r2 = z.normSquared();
    return {task.maxIter, r2, r2 > kBailoutRadiusSquared};
}

template <FractalFamily Family>
RawResult runFamily(std::span<const Complex> ref, const Task& task, int& skipped) noexcept
{
    return task.n == 2 ? run<Family, true>(ref, task, skipped)
                       : run<Family, false>(ref, task, skipped);
}

RawResult runAny(FractalFamily family, std::span<const Complex> ref, const Task& task,
                 int& skipped) noexcept
{
    switch (family)
    {
        case FractalFamily::MANDELBROT:
            return runFamily<FractalFamily::MANDELBROT>(ref, task, skipped);
        case FractalFamily::BURNING_SHIP:
            return runFamily<FractalFamily::BURNING_SHIP>(ref, task, skipped);
        case FractalFamily::TRICORN:
            return runFamily<FractalFamily::TRICORN>(ref, task, skipped);
    }
    return {task.maxIter, 0.0, false};
}

}  // namespace

IterationResult iteratePerturbed(const ReferenceOrbit& reference, Complex delta0, int maxIter,
                                 const BlaTable* bla, PerturbationStats* stats) noexcept
{
    const FractalSpec& spec = reference.spec();
    const Task         task{.delta0  = delta0,
                            .julia   = spec.julia,
                            .n       = clampExponent(spec.exponent),
                            .maxIter = reference.length() < 2 ? 0  // a lone Z_0: nothing to step
                                                              : std::max(maxIter, 0),
                            .bla     = bla};
    int                skipped = 0;
    const RawResult    raw     = runAny(spec.family, reference.points(), task, skipped);
    if (stats != nullptr)
    {
        *stats = {.iterations = raw.steps, .skipped = skipped};
    }
    return smoothIterationResult(raw.steps, raw.normSquared, raw.escaped, task.n);
}

}  // namespace mandelbrotter
