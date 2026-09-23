#include "Mandelbrotter/BlaTable.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

#include "Mandelbrotter/ReferenceOrbit.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter
{

double Mat2::operatorNorm() const noexcept
{
    // Closed form for 2x2: the larger singular value from the Frobenius norm and the determinant.
    const double sum          = a * a + b * b + c * c + d * d;
    const double det          = a * d - b * c;
    const double discriminant = std::max(sum * sum - 4.0 * det * det, 0.0);
    return std::sqrt((sum + std::sqrt(discriminant)) / 2.0);
}

namespace
{

constexpr double sign(double v) noexcept
{
    return v < 0.0 ? -1.0 : 1.0;
}

/// n w^(n-1): the derivative of w^n.
Complex powerDerivative(Complex w, int n) noexcept
{
    Complex wPow = w;
    for (int i = 2; i < n; ++i)
    {
        wPow = wPow * w;
    }
    return wPow * static_cast<double>(n);
}

}  // namespace

Bla singleStepBla(const ReferenceOrbit& reference, int index, double epsilon)
{
    const FractalSpec& spec = reference.spec();
    const int          n    = clampExponent(spec.exponent);
    const Complex      z    = reference.points()[static_cast<std::size_t>(index)];

    // w is the twisted point and `twist` what the family does to a delta before the power; for the
    // Burning Ship that is only linear while the delta does not cross a fold.
    Complex w         = z;
    Mat2    twist     = Mat2::identity();
    double  foldLimit = std::numeric_limits<double>::infinity();
    switch (spec.family)
    {
        case FractalFamily::MANDELBROT:
            break;
        case FractalFamily::TRICORN:
            w     = {z.re, -z.im};
            twist = {1.0, 0.0, 0.0, -1.0};
            break;
        case FractalFamily::BURNING_SHIP:
            w         = {std::abs(z.re), std::abs(z.im)};
            twist     = {sign(z.re), 0.0, 0.0, sign(z.im)};
            foldLimit = std::min(std::abs(z.re), std::abs(z.im));
            break;
    }
    // Dropping the second-order term of (w + dw)^n - w^n keeps the relative error below epsilon
    // while |dw| < 2 epsilon |w| / (n - 1); the twist preserves lengths.
    const double linearLimit = 2.0 * epsilon * std::hypot(w.re, w.im) / (n - 1);
    return {.m      = Mat2::scaling(powerDerivative(w, n)) * twist,
            .n      = spec.julia ? Mat2{} : Mat2::identity(),
            .radius = std::min(linearLimit, foldLimit),
            .length = 1};
}

Bla mergeBla(const Bla& x, const Bla& y, double maxDeltaC) noexcept
{
    // The delta handed to y is |Mx d + Nx dc| <= |Mx| |d| + |Nx| |dc|, which must stay below ry.
    const double slack   = y.radius - x.n.operatorNorm() * maxDeltaC;
    const double stretch = x.m.operatorNorm();
    double       radius  = 0.0;
    if (std::isfinite(slack) && std::isfinite(stretch) && slack > 0.0)
    {
        radius = std::min(x.radius, stretch > 0.0 ? slack / stretch : x.radius);
    }
    return {.m = y.m * x.m, .n = y.m * x.n + y.n, .radius = radius, .length = x.length + y.length};
}

BlaTable::BlaTable(const ReferenceOrbit& reference, double maxDeltaC, double epsilon)
{
    const int steps = reference.length() - 1;  // single steps start at indices 0 .. steps - 1
    if (steps < 2)
    {
        return;
    }
    m_levels.reserve(std::numeric_limits<std::uint32_t>::digits);

    // Level 1 straight from pairs of single steps, then every level from the one below it.
    std::vector<Bla> level;
    level.reserve(static_cast<std::size_t>(steps / 2));
    for (int i = 0; i + 1 < steps; i += 2)
    {
        level.push_back(mergeBla(singleStepBla(reference, i, epsilon),
                                 singleStepBla(reference, i + 1, epsilon), maxDeltaC));
    }
    m_levels.push_back(std::move(level));
    while (m_levels.back().size() >= 2)
    {
        const std::vector<Bla>& below = m_levels.back();
        std::vector<Bla>        next;
        next.reserve(below.size() / 2);
        for (std::size_t i = 0; i + 1 < below.size(); i += 2)
        {
            next.push_back(mergeBla(below[i], below[i + 1], maxDeltaC));
        }
        m_levels.push_back(std::move(next));
    }
}

const Bla* BlaTable::longestValid(int index, double deltaNormSquared, int maxLength) const noexcept
{
    if (index < 0 || m_levels.empty())
    {
        return nullptr;
    }
    // An entry of level j starts only at multiples of 2^j.
    const auto unsignedIndex = static_cast<std::uint32_t>(index);
    const int  alignment     = index == 0 ? levels() : std::countr_zero(unsignedIndex);
    for (int j = std::min(levels(), alignment); j >= 1; --j)
    {
        const std::vector<Bla>& level = m_levels[static_cast<std::size_t>(j - 1)];
        const std::size_t       i     = unsignedIndex >> j;
        if (i >= level.size())
        {
            continue;  // past the end of the orbit at this length
        }
        const Bla& bla = level[i];
        if (std::cmp_greater(bla.length, maxLength))
        {
            continue;
        }
        if (deltaNormSquared < bla.radius * bla.radius)
        {
            return &bla;
        }
    }
    return nullptr;
}

std::size_t BlaTable::entryCount() const noexcept
{
    return std::accumulate(
        m_levels.begin(), m_levels.end(), std::size_t{0},
        [](std::size_t sum, const std::vector<Bla>& level) { return sum + level.size(); });
}

}  // namespace mandelbrotter
