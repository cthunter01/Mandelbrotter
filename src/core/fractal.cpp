#include "Mandelbrotter/fractal.h"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string_view>

#include "Mandelbrotter/Viewport.h"

namespace mandelbrotter
{

namespace
{

constexpr std::array kFamilies{FractalFamily::MANDELBROT, FractalFamily::BURNING_SHIP,
                               FractalFamily::TRICORN};

}  // namespace

std::string_view toString(FractalFamily family) noexcept
{
    switch (family)
    {
        case FractalFamily::MANDELBROT:
            return "mandelbrot";
        case FractalFamily::BURNING_SHIP:
            return "burning-ship";
        case FractalFamily::TRICORN:
            return "tricorn";
    }
    return "mandelbrot";
}

std::string_view displayName(FractalFamily family) noexcept
{
    switch (family)
    {
        case FractalFamily::MANDELBROT:
            return "Mandelbrot";
        case FractalFamily::BURNING_SHIP:
            return "Burning Ship";
        case FractalFamily::TRICORN:
            return "Tricorn";
    }
    return "Mandelbrot";
}

std::optional<FractalFamily> parseFamily(std::string_view name) noexcept
{
    if (name == "mandelbrot")
    {
        return FractalFamily::MANDELBROT;
    }
    if (name == "burning-ship" || name == "burningship" || name == "burning_ship")
    {
        return FractalFamily::BURNING_SHIP;
    }
    if (name == "tricorn")
    {
        return FractalFamily::TRICORN;
    }
    return std::nullopt;
}

std::span<const FractalFamily> allFamilies() noexcept
{
    return kFamilies;
}

int clampExponent(int exponent) noexcept
{
    return std::clamp(exponent, kMinExponent, kMaxExponent);
}

ViewSpec defaultView(const FractalSpec& spec)
{
    if (spec.julia)
    {
        return {{0.0, 0.0}, 0.75};
    }
    switch (spec.family)
    {
        case FractalFamily::MANDELBROT:
            return spec.exponent == 2 ? ViewSpec{{-0.5, 0.0}, 1.0} : ViewSpec{{0.0, 0.0}, 1.0};
        case FractalFamily::BURNING_SHIP:
            return {{-0.4, -0.6}, 0.75};
        case FractalFamily::TRICORN:
            return {{-0.25, 0.0}, 0.8};
    }
    return {};
}

}  // namespace mandelbrotter
