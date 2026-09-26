#include "Mandelbrotter/app/scenes.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/flights.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::app
{

namespace
{

constexpr double kSeahorseZoom = 5000.0;

Bookmark bookmark(std::string name, FractalFamily family, Complex center, double zoom,
                  std::string_view palette)
{
    RenderSettings base;
    base.fractal.family   = family;
    base.coloring.palette = std::string(palette);
    return {.name = std::move(name), .settings = viewAt(std::move(base), center, zoom)};
}

}  // namespace

RenderSettings viewAt(RenderSettings base, Complex center, double zoom)
{
    base.view.zoom   = clampZoom(zoom);
    base.view.center = BigComplex::fromComplex(center, fractionBitsFor(base.view.zoom));
    return base;
}

RenderSettings mandelbrotDefault()
{
    RenderSettings settings;
    settings.view = defaultView(settings.fractal);
    return settings;
}

RenderSettings burningShipDefault()
{
    RenderSettings settings;
    settings.fractal.family = FractalFamily::BURNING_SHIP;
    settings.view           = defaultView(settings.fractal);
    return settings;
}

RenderSettings juliaExample()
{
    RenderSettings settings;
    settings.fractal.julia = true;
    settings.fractal.seed  = kJuliaSeed;
    settings.view          = defaultView(settings.fractal);
    return settings;
}

RenderSettings seahorse(std::string_view palette)
{
    RenderSettings settings   = viewAt(mandelbrotDefault(), kSeahorseValley, kSeahorseZoom);
    settings.coloring.palette = std::string(palette);
    return settings;
}

RenderSettings seahorseFire()
{
    RenderSettings settings   = seahorse("fire");
    settings.coloring.density = 32.0;
    settings.coloring.offset  = 0.25;
    return settings;
}

RenderSettings deepSeahorse(double zoom)
{
    const Flight* dive = findFlight("seahorse-dive");
    if (dive == nullptr || dive->keyframes.empty())
    {
        return seahorse();  // not reached: the flight is built in
    }
    RenderSettings settings = dive->keyframes.back().settings;
    settings.view.zoom      = zoom;
    settings.view.center    = settings.view.center.withFractionBits(fractionBitsFor(zoom));
    return settings;
}

std::vector<Bookmark> screenshotBookmarks()
{
    return {
        bookmark("Seahorse Valley", FractalFamily::MANDELBROT, kSeahorseValley, kSeahorseZoom,
                 "electric"),
        bookmark("Elephant Valley", FractalFamily::MANDELBROT, {0.275, 0.006}, 60.0, "fire"),
        bookmark("The little ship", FractalFamily::BURNING_SHIP, {-1.75, -0.03}, 40.0, "fire"),
    };
}

}  // namespace mandelbrotter::app
