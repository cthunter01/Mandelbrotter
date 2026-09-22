#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>

#include "Mandelbrotter/exporter.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/render_settings.h"

namespace mandelbrotter
{

/// Settings given explicitly on the command line; they override a --view file and the defaults.
struct CliOverrides
{
    std::optional<FractalFamily> family;
    std::optional<int>           exponent;
    std::optional<Complex>       julia;
    std::optional<Complex>       center;
    std::optional<double>        zoom;
    std::optional<int>           iterations;
    std::optional<std::string>   palette;

    bool operator==(const CliOverrides&) const = default;
};

struct CliOptions
{
    bool                                 help{false};
    std::optional<std::filesystem::path> renderOutput;
    std::optional<std::filesystem::path> viewFile;
    CliOverrides                         overrides;
    ExportOptions                        exportOptions;

    /// True when the program should open the window (no --help, no --render).
    [[nodiscard]] bool wantsGui() const noexcept { return !help && !renderOutput.has_value(); }
};

/// Parses the arguments after the program name. Does no I/O.
[[nodiscard]] std::expected<CliOptions, std::string> parseCommandLine(
    std::span<const std::string_view> args);

/// Applies the overrides on top of `base`. A family override without a view file or centre/zoom
/// starts from that family's default view.
[[nodiscard]] RenderSettings applyOverrides(RenderSettings base, const CliOverrides& overrides,
                                            bool baseIsDefault);

/// The settings to render or open: the --view file (if any) with the overrides applied.
[[nodiscard]] std::expected<RenderSettings, std::string> resolveSettings(const CliOptions& options);

[[nodiscard]] std::string usageText();

/// Runs the non-GUI part: prints usage for --help, or renders --render to a PNG. Returns the exit
/// code.
int runCli(const CliOptions& options, std::ostream& out, std::ostream& err);

}  // namespace mandelbrotter
