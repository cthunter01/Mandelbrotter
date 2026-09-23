#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "Mandelbrotter/RenderSettings.h"

namespace mandelbrotter
{

/// Links in the help book that act on the running application instead of opening a page:
/// "mandelbrotter:<verb> [arguments]", words separated by spaces.
inline constexpr std::string_view kHelpActionScheme = "mandelbrotter:";

/// "view <options>": the current view with command-line view options applied, e.g.
/// "mandelbrotter:view --center -0.7436,0.1318 --zoom 5000 --palette electric".
struct ViewAction
{
    std::vector<std::string> args;

    bool operator==(const ViewAction&) const = default;
};

/// "flight <id>": plays a built-in flight (flights.h).
struct FlightAction
{
    std::string id;

    bool operator==(const FlightAction&) const = default;
};

/// "tour": starts the guided tour.
struct TourAction
{
    bool operator==(const TourAction&) const = default;
};

/// "orbit on|off": the orbit overlay.
struct OrbitAction
{
    bool on{};

    bool operator==(const OrbitAction&) const = default;
};

/// "reset": back to the current fractal's default view.
struct ResetAction
{
    bool operator==(const ResetAction&) const = default;
};

/// "export": opens the Save image as PNG dialog.
struct ExportDialogAction
{
    bool operator==(const ExportDialogAction&) const = default;
};

using HelpAction = std::variant<ViewAction, FlightAction, TourAction, OrbitAction, ResetAction,
                                ExportDialogAction>;

/// True when `url` starts with kHelpActionScheme (it may still be malformed).
[[nodiscard]] bool isHelpActionUrl(std::string_view url) noexcept;

/// Parses a help action link. Repeated spaces are fine. Does no I/O.
[[nodiscard]] std::expected<HelpAction, std::string> parseHelpAction(std::string_view url);

/// The settings a view action opens: `current` with the action's options applied, as the command
/// line would apply them. A family or Julia change without --center/--zoom starts from that
/// fractal's default view; otherwise the current view is kept. Options that are not view options
/// (--render, --view, --help, --size, --supersample, --screenshots) are rejected.
[[nodiscard]] std::expected<RenderSettings, std::string> resolveViewAction(
    const RenderSettings& current, const ViewAction& action);

}  // namespace mandelbrotter
