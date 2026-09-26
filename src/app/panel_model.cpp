#include "Mandelbrotter/app/panel_model.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>

#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/parse.h"

namespace mandelbrotter::app
{

int offsetToSlider(double offset) noexcept
{
    const double phase = offset - std::floor(offset);
    return static_cast<int>(std::lround(phase * kOffsetSliderSteps));
}

double sliderToOffset(int position) noexcept
{
    return static_cast<double>(position) / kOffsetSliderSteps;
}

std::optional<Complex> parseSeed(std::string_view re, std::string_view im)
{
    const std::optional<double> real      = parseNumber<double>(trimSpaces(re));
    const std::optional<double> imaginary = parseNumber<double>(trimSpaces(im));
    if (!real || !imaginary)
    {
        return std::nullopt;
    }
    return Complex{*real, *imaginary};
}

std::optional<Complex> previewSeedFor(const FractalSpec&     spec,
                                      std::optional<Complex> hovered) noexcept
{
    if (spec.julia)
    {
        return spec.seed;
    }
    return hovered;
}

std::optional<std::size_t> familyIndex(FractalFamily family) noexcept
{
    const auto families = allFamilies();
    for (std::size_t i = 0; i < families.size(); ++i)
    {
        if (families[i] == family)
        {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> paletteIndex(std::string_view name) noexcept
{
    const auto names = paletteNames();
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        if (names[i] == name)
        {
            return i;
        }
    }
    return std::nullopt;
}

std::string effectiveLimitText(int iterations)
{
    return std::format("Effective limit: {}", iterations);
}

RenderSettings previewSettings(const FractalSpec& spec, Complex seed,
                               const ColoringSettings& coloring)
{
    RenderSettings settings;
    settings.fractal = {
        .family = spec.family, .exponent = spec.exponent, .julia = true, .seed = seed};
    settings.view           = defaultView(settings.fractal);
    settings.maxIterations  = kPreviewIterations;
    settings.autoIterations = false;
    settings.coloring       = coloring;
    return settings;
}

}  // namespace mandelbrotter::app
