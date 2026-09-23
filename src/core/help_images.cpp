#include "Mandelbrotter/help_images.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/exporter.h"
#include "Mandelbrotter/flights.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"
#include "Mandelbrotter/png_writer.h"

namespace mandelbrotter
{

namespace
{

constexpr Complex kSeahorseValley{-0.7436, 0.1318};

RenderSettings at(RenderSettings settings, Complex center, double zoom)
{
    settings.view.zoom   = clampZoom(zoom);
    settings.view.center = BigComplex::fromComplex(center, fractionBitsFor(settings.view.zoom));
    return settings;
}

RenderSettings family(FractalFamily family, int exponent = 2)
{
    RenderSettings settings;
    settings.fractal.family   = family;
    settings.fractal.exponent = exponent;
    settings.view             = defaultView(settings.fractal);
    return settings;
}

RenderSettings julia(Complex seed, std::string_view palette)
{
    RenderSettings settings;
    settings.fractal.julia    = true;
    settings.fractal.seed     = seed;
    settings.view             = defaultView(settings.fractal);
    settings.coloring.palette = std::string(palette);
    return settings;
}

RenderSettings seahorse(std::string_view palette, double zoom)
{
    RenderSettings settings   = at(RenderSettings{}, kSeahorseValley, zoom);
    settings.coloring.palette = std::string(palette);
    return settings;
}

RenderSettings fixedIterations(RenderSettings settings, int iterations)
{
    settings.maxIterations  = iterations;
    settings.autoIterations = false;
    return settings;
}

HelpImageSpec spec(std::string file, RenderSettings settings, PixelSize size = {400, 250})
{
    return {.file = std::move(file), .settings = std::move(settings), .size = size};
}

}  // namespace

std::vector<HelpImageSpec> helpImageSpecs()
{
    std::vector<HelpImageSpec> specs{
        spec("fractal-mandelbrot.png", family(FractalFamily::MANDELBROT)),
        spec("fractal-multibrot-3.png", family(FractalFamily::MANDELBROT, 3)),
        spec("fractal-burning-ship.png", family(FractalFamily::BURNING_SHIP)),
        spec("fractal-tricorn.png", family(FractalFamily::TRICORN)),
        spec("julia-a.png", julia({-0.8, 0.156}, "ocean")),
        spec("julia-b.png", julia({-0.4, 0.6}, "fire")),
        spec("julia-c.png", julia({0.285, 0.01}, "electric")),
        spec("julia-d.png", julia({-0.7269, 0.1889}, "rainbow")),
        spec("iterations-50.png", fixedIterations(seahorse("classic", 5000.0), 50)),
        spec("iterations-2000.png", fixedIterations(seahorse("classic", 5000.0), 2000)),
    };
    for (const std::string_view name : paletteNames())
    {
        specs.push_back(
            spec("palette-" + std::string(name) + ".png", seahorse(name, 2000.0), {320, 200}));
    }
    // The dives' destinations, so the thumbnails on the demos page always match the flights.
    for (const std::string_view id :
         {"seahorse-dive", "antenna-minibrot", "elephant-valley", "feigenbaum"})
    {
        const Flight* flight = findFlight(id);
        if (flight == nullptr || flight->keyframes.empty())
        {
            throw std::logic_error("help images: unknown flight " + std::string(id));
        }
        specs.push_back(
            spec("place-" + std::string(id) + ".png", flight->keyframes.back().settings));
    }
    {
        RenderSettings deep = findFlight("seahorse-dive")->keyframes.back().settings;
        deep.view.zoom      = 1e12;
        deep.view.center    = deep.view.center.withFractionBits(fractionBitsFor(deep.view.zoom));
        specs.push_back(spec("deep-1e12.png", deep));
    }
    return specs;
}

void writeHelpImages(const std::filesystem::path&                 dir,
                     const std::function<void(std::string_view)>& progress, double sizeScale)
{
    std::filesystem::create_directories(dir);
    for (const HelpImageSpec& image : helpImageSpecs())
    {
        if (progress)
        {
            progress(image.file);
        }
        const auto scaled = [sizeScale](int pixels) {
            return std::max(1, static_cast<int>(std::lround(pixels * sizeScale)));
        };
        const ExportOptions options{.size = {scaled(image.size.width), scaled(image.size.height)},
                                    .supersample = image.supersample};
        const std::optional<RgbImage> rendered = renderForExport(image.settings, options);
        if (!rendered)
        {
            throw std::runtime_error("help images: render of " + image.file + " was cancelled");
        }
        writePng(dir / image.file, *rendered);
    }
}

}  // namespace mandelbrotter
