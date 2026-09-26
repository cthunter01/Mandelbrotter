#include "Mandelbrotter/flights.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter
{

namespace
{

using std::chrono::milliseconds;
using std::chrono::seconds;

constexpr Complex kSeahorseValley{-0.7436, 0.1318};
/// A deep point in Seahorse Valley (the classic zoom sequence's destination); a minibrot sits at
/// zoom 1e13.
constexpr std::string_view kSeahorseDeepRe = "-0.743643887037158704752191506114774";
constexpr std::string_view kSeahorseDeepIm = "0.131825904205311970493132056385139";

RenderSettings mandelbrot(std::string_view palette)
{
    RenderSettings settings;
    settings.coloring.palette = std::string(palette);
    return settings;
}

RenderSettings at(RenderSettings settings, Complex center, double zoom)
{
    settings.view.zoom   = clampZoom(zoom);
    settings.view.center = BigComplex::fromComplex(center, fractionBitsFor(settings.view.zoom));
    return settings;
}

RenderSettings at(RenderSettings settings, std::string_view re, std::string_view im, double zoom)
{
    settings.view.zoom = clampZoom(zoom);
    const auto center  = BigComplex::fromDecimal(re, im, fractionBitsFor(settings.view.zoom));
    if (!center)
    {
        throw std::logic_error("flights: malformed centre");
    }
    settings.view.center = *center;
    return settings;
}

FlightKeyframe leg(RenderSettings settings, std::chrono::milliseconds duration,
                   Easing easing = Easing::SMOOTH)
{
    return {.settings = std::move(settings), .duration = duration, .easing = easing};
}

Flight seahorseDive()
{
    const RenderSettings start = mandelbrot("electric");
    return {.id    = "seahorse-dive",
            .title = "Dive into Seahorse Valley",
            .description =
                "From the whole set down to a minibrot at zoom 1e13; the status bar "
                "switches to deep rendering on the way.",
            .keyframes = {leg(start, milliseconds{0}),
                          leg(at(start, kSeahorseDeepRe, kSeahorseDeepIm, 1e13), seconds{43})}};
}

Flight antennaMinibrot()
{
    const RenderSettings start = mandelbrot("classic");
    // The period-3 island on the antenna, then the period-9 island on its own antenna.
    // Zoom 60 shows the whole island; from about 250 the screen is inside its body, all black.
    const RenderSettings island = at(start, {-1.7548776662466927, 0.0}, 60);
    return {.id    = "antenna-minibrot",
            .title = "The minibrot on the antenna",
            .description =
                "Along the real axis to the little copy of the set at -1.75, then on "
                "to the copy on its own antenna.",
            .keyframes = {leg(start, milliseconds{0}), leg(island, seconds{10}),
                          leg(island, seconds{2}),
                          leg(at(start, {-1.7864402555, 0.0}, 1e7), seconds{13})}};
}

Flight elephantValley()
{
    const RenderSettings start = mandelbrot("fire");
    return {
        .id          = "elephant-valley",
        .title       = "Elephant Valley spirals",
        .description = "Into the spirals on the right-hand side of the set, to zoom 1e7.",
        .keyframes = {leg(start, milliseconds{0}),
                      leg(at(start, {0.2549870375144766, -0.0005679790528465}, 1e7), seconds{23})}};
}

Flight feigenbaum()
{
    // Points near the Feigenbaum point escape slowly (the period doubles every 4.669x of zoom),
    // so the flight starts from a higher iteration base than usual and stops at 3e5.
    RenderSettings start = mandelbrot("classic");
    start.maxIterations  = 2500;
    return {.id    = "feigenbaum",
            .title = "The Feigenbaum cascade",
            .description =
                "Down the real axis into the point where the period-doubling pattern "
                "repeats itself at every scale.",
            .keyframes = {leg(start, milliseconds{0}),
                          leg(at(start, {-1.401155189092, 0.0}, 3e5), seconds{18})}};
}

Flight juliaSweep()
{
    constexpr int    kLegs    = 72;
    constexpr double kRadius  = 0.7885;
    RenderSettings   settings = mandelbrot("ocean");
    settings.fractal.julia    = true;
    settings.view             = defaultView(settings.fractal);

    Flight flight{.id    = "julia-sweep",
                  .title = "A walk around the Julia sets",
                  .description =
                      "The Julia set morphs as its constant travels once around the "
                      "circle |c| = 0.7885.",
                  .keyframes = {}};
    for (int i = 0; i <= kLegs; ++i)
    {
        const double angle    = 2.0 * std::numbers::pi * i / kLegs;
        settings.fractal.seed = {kRadius * std::cos(angle), kRadius * std::sin(angle)};
        flight.keyframes.push_back(leg(settings, milliseconds{i == 0 ? 0 : 500}, Easing::LINEAR));
    }
    return flight;
}

Flight paletteSweep()
{
    const RenderSettings settings = at(mandelbrot("classic"), kSeahorseValley, 5000.0);
    RenderSettings       end      = settings;
    end.coloring.offset           = 1.0;
    return {.id          = "palette-sweep",
            .title       = "Palette sweep",
            .description = "The same view while the palette cycles once through its colours.",
            .keyframes   = {leg(settings, milliseconds{0}), leg(end, seconds{12}, Easing::LINEAR)}};
}

const std::vector<Flight>& flights()
{
    static const std::vector<Flight> kFlights{seahorseDive(), antennaMinibrot(), elephantValley(),
                                              feigenbaum(),   juliaSweep(),      paletteSweep()};
    return kFlights;
}

double lerp(double a, double b, double u) noexcept
{
    return a + ((b - a) * u);
}

}  // namespace

std::span<const Flight> builtinFlights()
{
    return flights();
}

const Flight* findFlight(std::string_view id)
{
    for (const Flight& flight : flights())
    {
        if (flight.id == id)
        {
            return &flight;
        }
    }
    return nullptr;
}

std::chrono::milliseconds totalDuration(const Flight& flight) noexcept
{
    std::chrono::milliseconds total{};
    for (std::size_t i = 1; i < flight.keyframes.size(); ++i)
    {
        total += flight.keyframes[i].duration;
    }
    return total;
}

double ease(Easing easing, double u) noexcept
{
    u = std::clamp(u, 0.0, 1.0);
    return easing == Easing::SMOOTH ? u * u * (3.0 - (2.0 * u)) : u;
}

RenderSettings interpolateSettings(const RenderSettings& from, const RenderSettings& to, double u)
{
    u = std::clamp(u, 0.0, 1.0);
    if (u <= 0.0)
    {
        return from;
    }
    if (u >= 1.0)
    {
        return to;
    }
    RenderSettings out = to;  // the discrete fields switch at the start of the leg

    const double zoomFrom = from.view.zoom;
    const double zoomTo   = to.view.zoom;
    const double zoom     = zoomFrom * std::pow(zoomTo / zoomFrom, u);
    out.view.zoom         = zoom;

    // The target keeps shrinking towards the middle of the screen: its offset from the centre,
    // measured in screen widths, is (1 - u) times what it was at the start.
    const int        bits   = fractionBitsFor(std::max(zoomFrom, zoomTo));
    const BigComplex from0  = from.view.center.withFractionBits(bits);
    const BigComplex to0    = to.view.center.withFractionBits(bits);
    const double     factor = (1.0 - u) * zoomFrom / zoom;
    out.view.center =
        (to0 + (from0 - to0).scaled(factor, bits)).withFractionBits(fractionBitsFor(zoom));

    out.coloring.offset  = lerp(from.coloring.offset, to.coloring.offset, u);
    out.coloring.density = lerp(from.coloring.density, to.coloring.density, u);
    out.fractal.seed     = {lerp(from.fractal.seed.re, to.fractal.seed.re, u),
                            lerp(from.fractal.seed.im, to.fractal.seed.im, u)};
    out.maxIterations    = static_cast<int>(std::lround(
        lerp(static_cast<double>(from.maxIterations), static_cast<double>(to.maxIterations), u)));
    return out;
}

RenderSettings flightSettingsAt(const Flight& flight, std::chrono::milliseconds t)
{
    if (flight.keyframes.empty())
    {
        return {};
    }
    if (t <= milliseconds{0})
    {
        return flight.keyframes.front().settings;
    }
    std::chrono::milliseconds legStart{};
    for (std::size_t i = 1; i < flight.keyframes.size(); ++i)
    {
        const FlightKeyframe&           to     = flight.keyframes[i];
        const std::chrono::milliseconds legEnd = legStart + to.duration;
        if (t < legEnd)
        {
            const double u = static_cast<double>((t - legStart).count()) /
                             static_cast<double>(to.duration.count());
            return interpolateSettings(flight.keyframes[i - 1].settings, to.settings,
                                       ease(to.easing, u));
        }
        legStart = legEnd;
    }
    return flight.keyframes.back().settings;
}

}  // namespace mandelbrotter
