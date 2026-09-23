#pragma once

#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Mandelbrotter/RenderSettings.h"

namespace mandelbrotter
{

/// How a leg's progress maps onto time.
enum class Easing : std::uint8_t
{
    LINEAR,
    SMOOTH,  ///< smoothstep 3u^2 - 2u^3: gentle start and stop
};

/// Where a leg of a flight ends, and how long it takes to get there from the previous keyframe.
struct FlightKeyframe
{
    RenderSettings            settings;
    std::chrono::milliseconds duration{};  ///< ignored for the first keyframe; 0 = instant switch
    Easing                    easing{Easing::SMOOTH};

    bool operator==(const FlightKeyframe&) const = default;
};

/// A scripted animation: Help > Demos plays one, frame by frame, on the main window.
struct Flight
{
    std::string                 id;           ///< "seahorse-dive": menus and help links use it
    std::string                 title;        ///< the menu label
    std::string                 description;  ///< one sentence: menu help string, demos page
    std::vector<FlightKeyframe> keyframes;    ///< front() is where the flight starts
};

/// The flights offered in the Demos menu, in menu order.
[[nodiscard]] std::span<const Flight>   builtinFlights();
[[nodiscard]] const Flight*             findFlight(std::string_view id);
[[nodiscard]] std::chrono::milliseconds totalDuration(const Flight& flight) noexcept;

/// `u` clamped to [0, 1] and eased.
[[nodiscard]] double ease(Easing easing, double u) noexcept;

/// The settings a fraction `u` (already eased, clamped to [0, 1]) of the way from `from` to `to`:
/// the zoom moves exponentially, z(u) = z0 (z1/z0)^u; the centre follows
/// c(u) = c1 + (c0 - c1) (1 - u) z0 / z(u), so the target's offset on screen shrinks linearly to
/// zero (it never leaves the picture and the leg ends exactly on c1; plain linear motion when the
/// zoom does not change); palette offset and density, the Julia seed and the iteration limit move
/// linearly; family, exponent, Julia flag, palette and auto-iterations take `to`'s value as soon
/// as u > 0.
[[nodiscard]] RenderSettings interpolateSettings(const RenderSettings& from,
                                                 const RenderSettings& to, double u);

/// The settings `t` after the start; before 0 the first keyframe, after the end the last.
[[nodiscard]] RenderSettings flightSettingsAt(const Flight& flight, std::chrono::milliseconds t);

}  // namespace mandelbrotter
