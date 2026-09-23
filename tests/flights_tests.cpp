#include "Mandelbrotter/flights.h"

#include <chrono>
#include <cmath>
#include <set>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace
{

using mandelbrotter::BigComplex;
using mandelbrotter::Complex;
using mandelbrotter::ease;
using mandelbrotter::Easing;
using mandelbrotter::Flight;
using mandelbrotter::FlightKeyframe;
using mandelbrotter::flightSettingsAt;
using mandelbrotter::FractalFamily;
using mandelbrotter::interpolateSettings;
using mandelbrotter::RenderSettings;
using std::chrono::milliseconds;
using std::chrono::seconds;

RenderSettings at(Complex center, double zoom)
{
    RenderSettings settings;
    settings.view.zoom   = zoom;
    settings.view.center = BigComplex::fromComplex(center, mandelbrotter::fractionBitsFor(zoom));
    return settings;
}

TEST(Flights, EaseClampsAndSmooths)
{
    EXPECT_DOUBLE_EQ(ease(Easing::LINEAR, 0.25), 0.25);
    EXPECT_DOUBLE_EQ(ease(Easing::LINEAR, -1.0), 0.0);
    EXPECT_DOUBLE_EQ(ease(Easing::LINEAR, 2.0), 1.0);
    EXPECT_DOUBLE_EQ(ease(Easing::SMOOTH, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(ease(Easing::SMOOTH, 1.0), 1.0);
    EXPECT_DOUBLE_EQ(ease(Easing::SMOOTH, 0.5), 0.5);
    EXPECT_DOUBLE_EQ(ease(Easing::SMOOTH, 0.25), 0.15625);
}

TEST(Flights, ZoomMovesExponentially)
{
    const RenderSettings from = at({-0.5, 0.0}, 1.0);
    const RenderSettings to   = at({-0.7436, 0.1318}, 1e4);
    EXPECT_NEAR(interpolateSettings(from, to, 0.5).view.zoom, 100.0, 1e-9);
    EXPECT_NEAR(interpolateSettings(from, to, 0.25).view.zoom, 10.0, 1e-9);
}

TEST(Flights, EndpointsAreExact)
{
    const RenderSettings from = at({-0.5, 0.0}, 1.0);
    const RenderSettings to   = at({-0.7436, 0.1318}, 1e6);
    EXPECT_EQ(interpolateSettings(from, to, 0.0), from);
    EXPECT_EQ(interpolateSettings(from, to, 1.0), to);
    EXPECT_EQ(interpolateSettings(from, to, -0.5), from);
    EXPECT_EQ(interpolateSettings(from, to, 1.5), to);
}

TEST(Flights, TargetStaysOnScreenAndConvergesLinearly)
{
    const Complex        c0{-0.5, 0.0};
    const Complex        c1{-0.7436, 0.1318};
    const RenderSettings from = at(c0, 1.0);
    const RenderSettings to   = at(c1, 1e6);
    // The target's offset from the centre in screen units (complex offset times zoom) shrinks
    // linearly from its initial value to zero.
    const Complex initialOffset = (c1 - c0) * from.view.zoom;
    for (const double u : {0.25, 0.5, 0.75})
    {
        const RenderSettings mid    = interpolateSettings(from, to, u);
        const Complex        offset = (c1 - mid.view.center.approx()) * mid.view.zoom;
        EXPECT_NEAR(offset.re, initialOffset.re * (1.0 - u), 1e-8) << u;
        EXPECT_NEAR(offset.im, initialOffset.im * (1.0 - u), 1e-8) << u;
    }
}

TEST(Flights, CentreMovesLinearlyWhenTheZoomIsConstant)
{
    const RenderSettings from = at({0.0, 0.0}, 2.0);
    const RenderSettings to   = at({1.0, -2.0}, 2.0);
    const Complex        mid  = interpolateSettings(from, to, 0.25).view.center.approx();
    EXPECT_NEAR(mid.re, 0.25, 1e-12);
    EXPECT_NEAR(mid.im, -0.5, 1e-12);
}

TEST(Flights, DiscreteFieldsSwitchAtTheStartAndContinuousOnesInterpolate)
{
    RenderSettings from   = at({-0.5, 0.0}, 1.0);
    from.coloring.offset  = 0.0;
    from.coloring.density = 64.0;
    from.fractal.seed     = {0.0, 0.0};
    from.maxIterations    = 100;
    RenderSettings to     = at({-0.5, 0.0}, 1.0);
    to.fractal.family     = FractalFamily::BURNING_SHIP;
    to.fractal.exponent   = 3;
    to.fractal.julia      = true;
    to.fractal.seed       = {1.0, -1.0};
    to.coloring.palette   = "fire";
    to.coloring.offset    = 1.0;
    to.coloring.density   = 32.0;
    to.autoIterations     = false;
    to.maxIterations      = 300;

    const RenderSettings early = interpolateSettings(from, to, 0.01);
    EXPECT_EQ(early.fractal.family, FractalFamily::BURNING_SHIP);
    EXPECT_EQ(early.fractal.exponent, 3);
    EXPECT_TRUE(early.fractal.julia);
    EXPECT_EQ(early.coloring.palette, "fire");
    EXPECT_FALSE(early.autoIterations);

    const RenderSettings half = interpolateSettings(from, to, 0.5);
    EXPECT_DOUBLE_EQ(half.coloring.offset, 0.5);
    EXPECT_DOUBLE_EQ(half.coloring.density, 48.0);
    EXPECT_EQ(half.fractal.seed, (Complex{0.5, -0.5}));
    EXPECT_EQ(half.maxIterations, 200);
}

TEST(Flights, FlightSettingsAtWalksTheKeyframes)
{
    const RenderSettings a = at({-0.5, 0.0}, 1.0);
    const RenderSettings b = at({-0.7436, 0.1318}, 100.0);
    const RenderSettings c = at({-0.7436, 0.1318}, 1e4);
    const Flight         flight{
        .id          = "test",
        .title       = "Test",
        .description = "",
        .keyframes   = {{.settings = a, .duration = milliseconds{0}},
                        {.settings = b, .duration = seconds{2}},
                        {.settings = c, .duration = seconds{2}, .easing = Easing::LINEAR}}};
    EXPECT_EQ(mandelbrotter::totalDuration(flight), seconds{4});
    EXPECT_EQ(flightSettingsAt(flight, seconds{-1}), a);
    EXPECT_EQ(flightSettingsAt(flight, seconds{0}), a);
    EXPECT_EQ(flightSettingsAt(flight, seconds{1}),
              interpolateSettings(a, b, ease(Easing::SMOOTH, 0.5)));
    EXPECT_EQ(flightSettingsAt(flight, seconds{2}), b);
    EXPECT_EQ(flightSettingsAt(flight, seconds{3}), interpolateSettings(b, c, 0.5));
    EXPECT_EQ(flightSettingsAt(flight, seconds{4}), c);
    EXPECT_EQ(flightSettingsAt(flight, seconds{10}), c);
}

TEST(Flights, ZeroLengthLegsSwitchInstantly)
{
    const RenderSettings a = at({-0.5, 0.0}, 1.0);
    RenderSettings       b = a;
    b.coloring.palette     = "fire";
    RenderSettings c       = b;
    c.coloring.offset      = 1.0;
    const Flight flight{
        .id          = "test",
        .title       = "Test",
        .description = "",
        .keyframes   = {{.settings = a, .duration = milliseconds{0}},
                        {.settings = b, .duration = milliseconds{0}},
                        {.settings = c, .duration = seconds{2}, .easing = Easing::LINEAR}}};
    EXPECT_EQ(flightSettingsAt(flight, milliseconds{0}), a);
    const RenderSettings early = flightSettingsAt(flight, milliseconds{1});
    EXPECT_EQ(early.coloring.palette, "fire");
    EXPECT_NEAR(early.coloring.offset, 0.0005, 1e-9);
    EXPECT_DOUBLE_EQ(flightSettingsAt(flight, seconds{1}).coloring.offset, 0.5);
}

TEST(Flights, BuiltinFlightsAreWellFormed)
{
    const auto flights = mandelbrotter::builtinFlights();
    ASSERT_FALSE(flights.empty());
    std::set<std::string> ids;
    for (const Flight& flight : flights)
    {
        SCOPED_TRACE(flight.id);
        EXPECT_TRUE(ids.insert(flight.id).second) << "duplicate id";
        EXPECT_FALSE(flight.title.empty());
        EXPECT_FALSE(flight.description.empty());
        EXPECT_EQ(mandelbrotter::findFlight(flight.id), &flight);
        ASSERT_GE(flight.keyframes.size(), 2U);
        EXPECT_EQ(flight.keyframes.front().duration, milliseconds{0});
        for (const FlightKeyframe& keyframe : flight.keyframes)
        {
            EXPECT_GE(keyframe.settings.view.zoom, mandelbrotter::kMinZoom);
            EXPECT_LE(keyframe.settings.view.zoom, mandelbrotter::kMaxZoom);
            EXPECT_NE(mandelbrotter::findPalette(keyframe.settings.coloring.palette), nullptr);
            EXPECT_GE(keyframe.settings.fractal.exponent, mandelbrotter::kMinExponent);
            EXPECT_LE(keyframe.settings.fractal.exponent, mandelbrotter::kMaxExponent);
            EXPECT_GE(keyframe.duration, milliseconds{0});
        }
        EXPECT_GT(mandelbrotter::totalDuration(flight), milliseconds{0});
        EXPECT_LE(mandelbrotter::totalDuration(flight), seconds{60});
    }
    EXPECT_EQ(mandelbrotter::findFlight("no-such-flight"), nullptr);
}

TEST(Flights, DivesRunAtAboutOneDoublingPerSecond)
{
    for (const std::string_view id :
         {"seahorse-dive", "antenna-minibrot", "elephant-valley", "feigenbaum"})
    {
        const Flight* flight = mandelbrotter::findFlight(id);
        ASSERT_NE(flight, nullptr) << id;
        const double doublings = std::log2(flight->keyframes.back().settings.view.zoom /
                                           flight->keyframes.front().settings.view.zoom);
        const double seconds =
            std::chrono::duration<double>(mandelbrotter::totalDuration(*flight)).count();
        EXPECT_NEAR(doublings / seconds, 1.0, 0.25) << id;
    }
}

}  // namespace
