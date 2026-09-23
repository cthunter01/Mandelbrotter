#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Mandelbrotter/ReferenceOrbit.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter
{

/// Relative error allowed per linearised step: the dropped second-order term may be at most this
/// fraction of the linear one, so even a million skipped steps stay well below 1e-3 in the smooth
/// count. Larger values skip more and blur the last digits.
inline constexpr double kBlaEpsilon = 0x1p-30;

/// A real 2x2 matrix [a b; c d]: the derivative of one or more perturbed steps. Multiplication by a
/// complex number is the special case [re -im; im re]; the Tricorn's conjugation and the Burning
/// Ship's sign flips are real-linear but not complex-linear, which is why the table works in these.
struct Mat2
{
    double a{};
    double b{};
    double c{};
    double d{};

    [[nodiscard]] static constexpr Mat2 identity() noexcept { return {1.0, 0.0, 0.0, 1.0}; }
    /// Multiplication by w.
    [[nodiscard]] static constexpr Mat2 scaling(Complex w) noexcept
    {
        return {w.re, -w.im, w.im, w.re};
    }

    [[nodiscard]] constexpr Complex apply(Complex v) const noexcept
    {
        return {a * v.re + b * v.im, c * v.re + d * v.im};
    }
    /// this * other: `other` acts first.
    [[nodiscard]] constexpr Mat2 operator*(const Mat2& o) const noexcept
    {
        return {a * o.a + b * o.c, a * o.b + b * o.d, c * o.a + d * o.c, c * o.b + d * o.d};
    }
    [[nodiscard]] constexpr Mat2 operator+(const Mat2& o) const noexcept
    {
        return {a + o.a, b + o.b, c + o.c, d + o.d};
    }
    /// The most the matrix can stretch a vector (its spectral norm).
    [[nodiscard]] double operatorNorm() const noexcept;

    bool operator==(const Mat2&) const = default;
};

/// One bilinear approximation: delta' = m * delta + n * deltaC after `length` reference steps,
/// accurate while |delta| < radius at the start.
struct Bla
{
    Mat2          m;
    Mat2          n;
    double        radius{};
    std::uint32_t length{};
};

/// The linearisation of the single step from reference index `index` (what level 0 would hold).
[[nodiscard]] Bla singleStepBla(const ReferenceOrbit& reference, int index,
                                double epsilon = kBlaEpsilon);
/// x followed by y, valid for pixels with |deltaC| <= maxDeltaC.
[[nodiscard]] Bla mergeBla(const Bla& x, const Bla& y, double maxDeltaC) noexcept;

/// Composed perturbed steps over a reference orbit (Zhuoran's bilinear approximation): level j
/// holds one entry per 2^j-aligned start index, covering 2^j steps. A pixel whose delta is within
/// an entry's radius jumps those steps with two matrix products instead of iterating them; where no
/// entry is valid the delta is no longer small against the orbit, and ordinary steps take over.
class BlaTable
{
public:
    /// Builds the table for pixels whose |deltaC| is at most `maxDeltaC` (0 in Julia mode).
    /// Level 0 is used for the build and not kept.
    BlaTable(const ReferenceOrbit& reference, double maxDeltaC, double epsilon = kBlaEpsilon);

    /// The longest entry starting at reference index `index`, at most `maxLength` steps long,
    /// whose radius exceeds the delta's norm (given squared); nullptr when there is none.
    [[nodiscard]] const Bla* longestValid(int index, double deltaNormSquared,
                                          int maxLength) const noexcept;

    /// The longest entries cover 2^levels() steps; 0 when the orbit has fewer than two steps.
    [[nodiscard]] int         levels() const noexcept { return static_cast<int>(m_levels.size()); }
    [[nodiscard]] std::size_t entryCount() const noexcept;

private:
    std::vector<std::vector<Bla>> m_levels;  ///< m_levels[j - 1] is level j
};

}  // namespace mandelbrotter
