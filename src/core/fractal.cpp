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

constexpr std::array kFamilies{FractalFamily::Mandelbrot, FractalFamily::BurningShip,
                               FractalFamily::Tricorn};

}  // namespace

std::string_view toString(FractalFamily family) noexcept
{
    switch (family)
    {
        case FractalFamily::Mandelbrot:
            return "mandelbrot";
        case FractalFamily::BurningShip:
            return "burning-ship";
        case FractalFamily::Tricorn:
            return "tricorn";
    }
    return "mandelbrot";
}

std::string_view displayName(FractalFamily family) noexcept
{
    switch (family)
    {
        case FractalFamily::Mandelbrot:
            return "Mandelbrot";
        case FractalFamily::BurningShip:
            return "Burning Ship";
        case FractalFamily::Tricorn:
            return "Tricorn";
    }
    return "Mandelbrot";
}

std::optional<FractalFamily> parseFamily(std::string_view name) noexcept
{
    if (name == "mandelbrot")
    {
        return FractalFamily::Mandelbrot;
    }
    if (name == "burning-ship" || name == "burningship" || name == "burning_ship")
    {
        return FractalFamily::BurningShip;
    }
    if (name == "tricorn")
    {
        return FractalFamily::Tricorn;
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

ViewSpec defaultView(const FractalSpec& spec) noexcept
{
    if (spec.julia)
    {
        return {{0.0, 0.0}, 0.75};
    }
    switch (spec.family)
    {
        case FractalFamily::Mandelbrot:
            return spec.exponent == 2 ? ViewSpec{{-0.5, 0.0}, 1.0} : ViewSpec{{0.0, 0.0}, 1.0};
        case FractalFamily::BurningShip:
            return {{-0.4, -0.6}, 0.75};
        case FractalFamily::Tricorn:
            return {{-0.25, 0.0}, 0.8};
    }
    return {};
}

}  // namespace mandelbrotter
