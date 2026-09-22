#pragma once

#include <vector>

#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter
{

/// Escape radius. Large on purpose: the smooth iteration count is more accurate the further out z
/// gets.
inline constexpr double kBailoutRadius        = 256.0;
inline constexpr double kBailoutRadiusSquared = kBailoutRadius * kBailoutRadius;

struct IterationResult
{
    /// Continuous escape time (steps taken, minus a fraction for how far past the bailout z ended
    /// up). Equals maxIter for interior points.
    double smoothIter{};
    bool   interior{};

    bool operator==(const IterationResult&) const = default;
};

/// The point where iteration starts and the constant added each step, for the pixel at p.
struct OrbitStart
{
    Complex z0;
    Complex c;
};
[[nodiscard]] OrbitStart orbitStart(const FractalSpec& spec, Complex p) noexcept;

/// Iterate z <- f(z) + c starting from z0, for at most maxIter steps.
[[nodiscard]] IterationResult iterate(const FractalSpec& spec, Complex z0, Complex c,
                                      int maxIter) noexcept;

/// iterate() for the pixel p (z0 = p, c = seed in Julia mode; z0 = 0, c = p otherwise).
[[nodiscard]] IterationResult iteratePixel(const FractalSpec& spec, Complex p,
                                           int maxIter) noexcept;

/// The orbit of the pixel p: z0, z1, ... up to and including the first escaped point, at most
/// maxPoints.
[[nodiscard]] std::vector<Complex> orbit(const FractalSpec& spec, Complex p, int maxPoints);

}  // namespace mandelbrotter
