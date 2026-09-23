#include "Mandelbrotter/ReferenceOrbit.h"

#include <algorithm>
#include <cstddef>
#include <stop_token>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/kernel.h"

namespace mandelbrotter
{

namespace
{

constexpr int kStopCheckInterval = 256;

/// The per-family twist applied before raising to the power (as in kernel.cpp, in big numbers).
BigComplex transform(FractalFamily family, const BigComplex& z)
{
    switch (family)
    {
        case FractalFamily::BURNING_SHIP:
            return z.absParts();
        case FractalFamily::TRICORN:
            return z.conj();
        case FractalFamily::MANDELBROT:
            return z;
    }
    return z;
}

BigComplex power(const BigComplex& w, int n)
{
    if (n == 2)
    {
        return w.squared();
    }
    BigComplex result = w;
    for (int i = 1; i < n; ++i)
    {
        result = result * w;
    }
    return result;
}

}  // namespace

ReferenceOrbit::ReferenceOrbit(const FractalSpec& spec, const BigComplex& center, int maxIter,
                               const std::stop_token& stop)
  : m_spec(spec)
{
    maxIter               = std::max(maxIter, 0);
    const int        n    = clampExponent(spec.exponent);
    const int        bits = center.fractionBits();
    const BigComplex c    = spec.julia ? BigComplex::fromComplex(spec.seed, bits) : center;
    BigComplex       z    = spec.julia ? center : BigComplex{bits};
    m_points.reserve(static_cast<std::size_t>(maxIter) + 1);
    for (int k = 0;; ++k)
    {
        m_points.push_back(z.approx());
        if (m_points.back().normSquared() > kBailoutRadiusSquared)
        {
            m_escaped = true;
            return;
        }
        if (k == maxIter)
        {
            return;
        }
        if (k % kStopCheckInterval == 0 && stop.stop_requested())
        {
            m_cancelled = true;
            return;
        }
        z = power(transform(spec.family, z), n) + c;
    }
}

}  // namespace mandelbrotter
