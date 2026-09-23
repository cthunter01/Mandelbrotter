#include "Mandelbrotter/help_action.h"

#include <expected>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/cli.h"
#include "Mandelbrotter/exporter.h"

namespace mandelbrotter
{

namespace
{

using Error = std::unexpected<std::string>;

std::vector<std::string_view> splitWords(std::string_view text)
{
    std::vector<std::string_view> words;
    while (!text.empty())
    {
        const auto start = text.find_first_not_of(" \t");
        if (start == std::string_view::npos)
        {
            break;
        }
        text.remove_prefix(start);
        const auto end = text.find_first_of(" \t");
        words.push_back(text.substr(0, end));
        text.remove_prefix(end == std::string_view::npos ? text.size() : end);
    }
    return words;
}

std::expected<HelpAction, std::string> expectNoArgs(std::string_view                  verb,
                                                    std::span<const std::string_view> args,
                                                    HelpAction                        action)
{
    if (!args.empty())
    {
        return Error(std::format("{} takes no arguments", verb));
    }
    return action;
}

}  // namespace

bool isHelpActionUrl(std::string_view url) noexcept
{
    return url.starts_with(kHelpActionScheme);
}

std::expected<HelpAction, std::string> parseHelpAction(std::string_view url)
{
    if (!isHelpActionUrl(url))
    {
        return Error(std::format("not a {} link: \"{}\"", kHelpActionScheme, url));
    }
    const std::vector<std::string_view> words = splitWords(url.substr(kHelpActionScheme.size()));
    if (words.empty())
    {
        return Error("empty help action");
    }
    const std::string_view                  verb = words.front();
    const std::span<const std::string_view> args(words.begin() + 1, words.end());

    if (verb == "view")
    {
        if (args.empty())
        {
            return Error("view needs at least one option, e.g. view --zoom 40");
        }
        return ViewAction{.args = {args.begin(), args.end()}};
    }
    if (verb == "flight")
    {
        if (args.size() != 1)
        {
            return Error("flight needs exactly one flight id");
        }
        return FlightAction{.id = std::string(args.front())};
    }
    if (verb == "orbit")
    {
        if (args.size() != 1 || (args.front() != "on" && args.front() != "off"))
        {
            return Error("orbit expects on or off");
        }
        return OrbitAction{.on = args.front() == "on"};
    }
    if (verb == "tour")
    {
        return expectNoArgs(verb, args, TourAction{});
    }
    if (verb == "reset")
    {
        return expectNoArgs(verb, args, ResetAction{});
    }
    if (verb == "export")
    {
        return expectNoArgs(verb, args, ExportDialogAction{});
    }
    return Error(std::format("unknown help action \"{}\"", verb));
}

std::expected<RenderSettings, std::string> resolveViewAction(const RenderSettings& current,
                                                             const ViewAction&     action)
{
    const std::vector<std::string_view> args(action.args.begin(), action.args.end());
    const auto                          options = parseCommandLine(args);
    if (!options)
    {
        return Error(options.error());
    }
    if (options->help || options->renderOutput || options->viewFile || options->screenshotsDir ||
        options->exportOptions != ExportOptions{})
    {
        return Error(
            "a view link may only carry view options (--fractal, --exponent, --julia, --center, "
            "--zoom, --iterations, --palette)");
    }
    const CliOverrides& overrides = options->overrides;
    const bool fractalChanges = (overrides.family && *overrides.family != current.fractal.family) ||
                                (overrides.julia && !current.fractal.julia);
    const bool startFromDefault = fractalChanges && !overrides.center && !overrides.zoom;
    return applyOverrides(current, overrides, startFromDefault);
}

}  // namespace mandelbrotter
