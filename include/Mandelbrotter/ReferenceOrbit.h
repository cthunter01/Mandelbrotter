#pragma once

#include <span>
#include <stop_token>
#include <vector>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter
{

/// The orbit of one point, computed in BigComplex and stored as doubles: what every pixel's small
/// delta is measured against in perturbation rendering (see perturbation.h), and the exact orbit of
/// any point deep in a zoom.
///
/// The points are Z_0, Z_1, ..., ending either with the first point past the bailout radius
/// (escaped()) or with Z_maxIter. Z_0 is 0, or the point itself in Julia mode.
class ReferenceOrbit
{
public:
    /// Iterates `center` under `spec` for at most `maxIter` steps. Looks at `stop` every few
    /// hundred steps and keeps what it has when stopping is requested (cancelled()).
    ReferenceOrbit(const FractalSpec& spec, const BigComplex& center, int maxIter,
                   const std::stop_token& stop = {});

    [[nodiscard]] const FractalSpec&       spec() const noexcept { return m_spec; }
    [[nodiscard]] std::span<const Complex> points() const noexcept { return m_points; }
    [[nodiscard]] int  length() const noexcept { return static_cast<int>(m_points.size()); }
    [[nodiscard]] bool escaped() const noexcept { return m_escaped; }
    [[nodiscard]] bool cancelled() const noexcept { return m_cancelled; }

private:
    FractalSpec          m_spec;
    std::vector<Complex> m_points;
    bool                 m_escaped{false};
    bool                 m_cancelled{false};
};

}  // namespace mandelbrotter
