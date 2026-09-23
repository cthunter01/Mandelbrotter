#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter
{

/// One of the rendered example pictures in the help book (docs/help/images), as opposed to the
/// screenshots of the window, which the GUI captures.
struct HelpImageSpec
{
    std::string    file;  ///< relative to the images directory, e.g. "fractal-tricorn.png"
    RenderSettings settings;
    PixelSize      size{400, 250};
    int            supersample{2};
};

/// Every rendered example picture: fractal-*, julia-*, palette-*, iterations-*, place-* (the
/// destinations of the built-in flights) and deep-*.
[[nodiscard]] std::vector<HelpImageSpec> helpImageSpecs();

/// Renders every spec into dir/<file> (the directory is created). `progress` is called with each
/// file name before it is rendered; `sizeScale` shrinks every picture (tests render tiny ones).
/// Throws on write errors.
void writeHelpImages(const std::filesystem::path&                 dir,
                     const std::function<void(std::string_view)>& progress  = {},
                     double                                       sizeScale = 1.0);

}  // namespace mandelbrotter
