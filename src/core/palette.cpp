#include "Mandelbrotter/palette.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Mandelbrotter/kernel.h"

namespace mandelbrotter
{

namespace
{

constexpr std::size_t kLutSize = 4096;

std::uint8_t lerpChannel(std::uint8_t a, std::uint8_t b, double t) noexcept
{
    const double value = std::lerp(static_cast<double>(a), static_cast<double>(b), t);
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

Rgb lerp(Rgb a, Rgb b, double t) noexcept
{
    return {lerpChannel(a.r, b.r, t), lerpChannel(a.g, b.g, t), lerpChannel(a.b, b.b, t)};
}

/// Wraps t into [0, 1).
double fract(double t) noexcept
{
    const double f = t - std::floor(t);
    return f >= 1.0 ? 0.0 : f;  // guards against rounding producing exactly 1.0
}

std::vector<Palette> makeBuiltinPalettes()
{
    std::vector<Palette> palettes;
    palettes.emplace_back("classic", std::vector<ColorStop>{{0.0, {0, 7, 100}},
                                                            {0.16, {32, 107, 203}},
                                                            {0.42, {237, 255, 255}},
                                                            {0.6425, {255, 170, 0}},
                                                            {0.8575, {0, 2, 0}}});
    palettes.emplace_back("grayscale",
                          std::vector<ColorStop>{{0.0, {0, 0, 0}}, {0.5, {255, 255, 255}}});
    palettes.emplace_back("fire", std::vector<ColorStop>{{0.0, {0, 0, 0}},
                                                         {0.25, {140, 10, 0}},
                                                         {0.5, {255, 130, 0}},
                                                         {0.7, {255, 240, 180}},
                                                         {0.9, {70, 0, 0}}});
    palettes.emplace_back("ocean", std::vector<ColorStop>{{0.0, {0, 0, 40}},
                                                          {0.3, {0, 60, 140}},
                                                          {0.55, {0, 170, 200}},
                                                          {0.75, {220, 250, 255}},
                                                          {0.9, {0, 50, 100}}});
    palettes.emplace_back("rainbow", std::vector<ColorStop>{{0.0, {255, 0, 0}},
                                                            {1.0 / 6.0, {255, 255, 0}},
                                                            {2.0 / 6.0, {0, 255, 0}},
                                                            {3.0 / 6.0, {0, 255, 255}},
                                                            {4.0 / 6.0, {0, 0, 255}},
                                                            {5.0 / 6.0, {255, 0, 255}}});
    palettes.emplace_back("electric", std::vector<ColorStop>{{0.0, {0, 0, 0}},
                                                             {0.2, {20, 0, 120}},
                                                             {0.45, {120, 0, 255}},
                                                             {0.65, {0, 200, 255}},
                                                             {0.8, {255, 255, 255}},
                                                             {0.92, {60, 0, 160}}});
    return palettes;
}

}  // namespace

Palette::Palette(std::string name, std::vector<ColorStop> stops)
  : m_name(std::move(name)), m_stops(std::move(stops))
{
    if (m_stops.empty())
    {
        m_stops.push_back({0.0, kBlack});
    }
    std::ranges::sort(m_stops, {}, &ColorStop::position);
    m_stops.front().position = 0.0;
    for (ColorStop& stop : m_stops)
    {
        stop.position = std::clamp(stop.position, 0.0, 1.0);
    }
    m_lut.reserve(kLutSize);
    for (std::size_t i = 0; i < kLutSize; ++i)
    {
        m_lut.push_back(interpolate(static_cast<double>(i) / kLutSize));
    }
}

Rgb Palette::interpolate(double t) const noexcept
{
    // Find the last stop at or before t; the segment runs from it to the next stop (or wraps to the
    // first at 1).
    auto         next         = std::ranges::upper_bound(m_stops, t, {}, &ColorStop::position);
    const auto   current      = std::prev(next);
    const double segmentStart = current->position;
    const double segmentEnd   = next == m_stops.end() ? 1.0 : next->position;
    const Rgb    endColor     = next == m_stops.end() ? m_stops.front().color : next->color;
    const double length       = segmentEnd - segmentStart;
    if (length <= 0.0)
    {
        return current->color;
    }
    return lerp(current->color, endColor, (t - segmentStart) / length);
}

Rgb Palette::sample(double t) const noexcept
{
    if (!std::isfinite(t))
    {
        return m_lut.front();
    }
    const auto index = static_cast<std::size_t>(fract(t) * kLutSize);
    return m_lut[std::min(index, kLutSize - 1)];
}

Rgb colorFor(IterationResult result, const Palette& palette,
             const ColoringSettings& settings) noexcept
{
    if (result.interior)
    {
        return kBlack;
    }
    const double density = std::clamp(settings.density, kMinDensity, kMaxDensity);
    return palette.sample(result.smoothIter / density + settings.offset);
}

std::span<const Palette> builtinPalettes() noexcept
{
    static const std::vector<Palette> kPalettes = makeBuiltinPalettes();
    return kPalettes;
}

std::span<const std::string_view> paletteNames() noexcept
{
    static const std::vector<std::string_view> kNames = [] {
        std::vector<std::string_view> names;
        for (const Palette& palette : builtinPalettes())
        {
            names.push_back(palette.name());
        }
        return names;
    }();
    return kNames;
}

const Palette* findPalette(std::string_view name) noexcept
{
    const auto palettes = builtinPalettes();
    const auto it       = std::ranges::find(palettes, name, &Palette::name);
    return it == palettes.end() ? nullptr : &*it;
}

const Palette& paletteOrDefault(std::string_view name) noexcept
{
    const Palette* palette = findPalette(name);
    return palette != nullptr ? *palette : builtinPalettes().front();
}

}  // namespace mandelbrotter
