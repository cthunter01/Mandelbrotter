#pragma once

#include "Mandelbrotter/BlaTable.h"
#include "Mandelbrotter/ReferenceOrbit.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/kernel.h"

namespace mandelbrotter
{

/// How a perturbed iteration went: the steps it advanced in total, and how many of those were
/// covered by BLA jumps rather than computed one by one.
struct PerturbationStats
{
    int iterations{};
    int skipped{};
};

/// Iterates a pixel as a small delta from a reference orbit instead of as an absolute position:
/// the delta stays accurate in double at any zoom while the reference carries the precision.
/// `delta0` is the pixel's offset from the orbit's point: an offset in c, or in z0 in Julia mode.
///
/// Rebasing (Zhuoran, 2022): whenever the pixel's orbit passes closer to zero than its delta is
/// long, or the reference runs out, the delta is re-expressed against the reference's start. That
/// keeps the delta small without a second reference, and lets any reference serve any pixel, only
/// more slowly the further apart they are.
///
/// With a BlaTable built on the same reference, stretches where the delta is tiny against the orbit
/// are jumped in one go (see BlaTable.h). Same result contract as iterate().
[[nodiscard]] IterationResult iteratePerturbed(const ReferenceOrbit& reference, Complex delta0,
                                               int maxIter, const BlaTable* bla = nullptr,
                                               PerturbationStats* stats = nullptr) noexcept;

/// |c + d| - |c| without cancellation: how the Burning Ship's fold acts on a delta.
[[nodiscard]] constexpr double diffAbs(double c, double d) noexcept
{
    if (c >= 0.0)
    {
        return c + d >= 0.0 ? d : -(2.0 * c + d);
    }
    return c + d <= 0.0 ? -d : 2.0 * c + d;
}

}  // namespace mandelbrotter
