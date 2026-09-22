#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Mandelbrotter/kernel.h"

namespace mandelbrotter
{

struct Rgb
{
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};

    bool operator==(const Rgb&) const = default;
};

inline constexpr Rgb kBlack{0, 0, 0};

struct ColorStop
{
    double position{};  ///< in [0, 1]
    Rgb    color{};
};

/// A cyclic colour gradient: sample(t) wraps t into [0, 1) and interpolates linearly between the
/// stops, and from the last stop back to the first at t = 1.
class Palette
{
public:
    /// `stops` must be sorted by position with positions in [0, 1]; the first stop is treated as
    /// being at 0.
    Palette(std::string name, std::vector<ColorStop> stops);

    [[nodiscard]] const std::string&         name() const noexcept { return m_name; }
    [[nodiscard]] std::span<const ColorStop> stops() const noexcept { return m_stops; }
    [[nodiscard]] Rgb                        sample(double t) const noexcept;

private:
    [[nodiscard]] Rgb interpolate(double t) const noexcept;

    std::string            m_name;
    std::vector<ColorStop> m_stops;
    std::vector<Rgb>       m_lut;
};

/// How iteration counts become colours.
struct ColoringSettings
{
    std::string palette{"classic"};
    double      density{64.0};  ///< iterations per palette cycle
    double      offset{0.0};    ///< cycle phase shift, in cycles

    bool operator==(const ColoringSettings&) const = default;
};

inline constexpr double kMinDensity = 1.0;
inline constexpr double kMaxDensity = 4096.0;

/// Interior points are black; the rest map smoothIter / density + offset onto the palette cycle.
[[nodiscard]] Rgb colorFor(IterationResult result, const Palette& palette,
                           const ColoringSettings& settings) noexcept;

/// The built-in palettes.
[[nodiscard]] std::span<const Palette>          builtinPalettes() noexcept;
[[nodiscard]] std::span<const std::string_view> paletteNames() noexcept;
[[nodiscard]] const Palette*                    findPalette(std::string_view name) noexcept;
/// findPalette, falling back to the first built-in palette for unknown names.
[[nodiscard]] const Palette& paletteOrDefault(std::string_view name) noexcept;

}  // namespace mandelbrotter
