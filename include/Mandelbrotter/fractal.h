#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter
{

/// The iteration formula z <- f(z) + c. Every family takes an integer exponent.
enum class FractalFamily : std::uint8_t
{
    MANDELBROT,    ///< f(z) = z^n (n = 2 is the classic set, n > 2 the "Multibrot" sets)
    BURNING_SHIP,  ///< f(z) = (|Re z| + i |Im z|)^n
    TRICORN,       ///< f(z) = conj(z)^n
};

inline constexpr int kMinExponent = 2;
inline constexpr int kMaxExponent = 8;

/// Which fractal to draw. In Julia mode the pixel is the starting point z0 and `seed` is the
/// constant c; otherwise z0 = 0 and the pixel is c.
struct FractalSpec
{
    FractalFamily family{FractalFamily::MANDELBROT};
    int           exponent{2};
    bool          julia{false};
    Complex       seed{};

    bool operator==(const FractalSpec&) const = default;
};

/// Machine-readable name: "mandelbrot", "burning-ship", "tricorn".
[[nodiscard]] std::string_view toString(FractalFamily family) noexcept;
/// Human-readable name for the UI: "Mandelbrot", "Burning Ship", "Tricorn".
[[nodiscard]] std::string_view displayName(FractalFamily family) noexcept;
/// Inverse of toString (case-sensitive). Also accepts "burningship" and "burning_ship".
[[nodiscard]] std::optional<FractalFamily> parseFamily(std::string_view name) noexcept;
/// Every family, in UI order.
[[nodiscard]] std::span<const FractalFamily> allFamilies() noexcept;

/// The exponent clamped to [kMinExponent, kMaxExponent].
[[nodiscard]] int clampExponent(int exponent) noexcept;

/// A sensible starting view that shows the whole set.
[[nodiscard]] ViewSpec defaultView(const FractalSpec& spec) noexcept;

}  // namespace mandelbrotter
