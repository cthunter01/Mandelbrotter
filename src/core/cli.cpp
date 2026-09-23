#include "Mandelbrotter/cli.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <exception>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/BigFixed.h"
#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/exporter.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"
#include "Mandelbrotter/parse.h"
#include "Mandelbrotter/png_writer.h"

namespace mandelbrotter
{

namespace
{

using Error   = std::unexpected<std::string>;
using Outcome = std::expected<void, std::string>;

std::expected<Complex, std::string> parseComplex(std::string_view flag, std::string_view text)
{
    const auto comma = text.find(',');
    const auto re =
        comma == std::string_view::npos ? std::nullopt : parseNumber<double>(text.substr(0, comma));
    const auto im = comma == std::string_view::npos ? std::nullopt
                                                    : parseNumber<double>(text.substr(comma + 1));
    if (!re || !im)
    {
        return Error(std::format("{} expects re,im (got \"{}\")", flag, text));
    }
    return Complex{*re, *im};
}

/// "re,im" for --center: decimals of any length, at the given precision.
std::optional<BigComplex> parseCenter(std::string_view text, int fractionBits)
{
    const auto comma = text.find(',');
    if (comma == std::string_view::npos)
    {
        return std::nullopt;
    }
    const auto re = BigFixed::fromDecimal(text.substr(0, comma), fractionBits);
    const auto im = BigFixed::fromDecimal(text.substr(comma + 1), fractionBits);
    if (!re || !im)
    {
        return std::nullopt;
    }
    return BigComplex{*re, *im};
}

std::string paletteList()
{
    std::string list;
    for (const auto name : paletteNames())
    {
        list += list.empty() ? "" : ", ";
        list += name;
    }
    return list;
}

// ---- one handler per option
// -------------------------------------------------------------------------------------

Outcome setRender(std::string_view value, CliOptions& options)
{
    options.renderOutput = std::filesystem::path(value);
    return {};
}

Outcome setView(std::string_view value, CliOptions& options)
{
    options.viewFile = std::filesystem::path(value);
    return {};
}

Outcome setFractal(std::string_view value, CliOptions& options)
{
    const auto family = parseFamily(value);
    if (!family)
    {
        return Error(std::format(
            "unknown fractal \"{}\" (expected mandelbrot, burning-ship or tricorn)", value));
    }
    options.overrides.family = family;
    return {};
}

Outcome setExponent(std::string_view value, CliOptions& options)
{
    const auto n = parseNumber<int>(value);
    if (!n || *n < kMinExponent || *n > kMaxExponent)
    {
        return Error(
            std::format("--exponent expects an integer from {} to {}", kMinExponent, kMaxExponent));
    }
    options.overrides.exponent = n;
    return {};
}

Outcome setJulia(std::string_view value, CliOptions& options)
{
    const auto seed = parseComplex("--julia", value);
    if (!seed)
    {
        return Error(seed.error());
    }
    options.overrides.julia = *seed;
    return {};
}

Outcome setCenter(std::string_view value, CliOptions& options)
{
    // Validated now (the range check does not depend on the precision), parsed in applyOverrides.
    if (!parseCenter(value, BigFixed::kMinFractionBits))
    {
        return Error(std::format("--center expects re,im (got \"{}\")", value));
    }
    options.overrides.center = std::string(value);
    return {};
}

Outcome setZoom(std::string_view value, CliOptions& options)
{
    const auto zoom = parseNumber<double>(value);
    if (!zoom || !(*zoom > 0.0))
    {
        return Error(std::format("--zoom expects a positive number (got \"{}\")", value));
    }
    options.overrides.zoom = zoom;
    return {};
}

Outcome setIterations(std::string_view value, CliOptions& options)
{
    const auto n = parseNumber<int>(value);
    if (!n || *n < kMinIterations || *n > kMaxIterations)
    {
        return Error(std::format("--iterations expects an integer from {} to {}", kMinIterations,
                                 kMaxIterations));
    }
    options.overrides.iterations = n;
    return {};
}

Outcome setPalette(std::string_view value, CliOptions& options)
{
    if (findPalette(value) == nullptr)
    {
        return Error(std::format("unknown palette \"{}\" (available: {})", value, paletteList()));
    }
    options.overrides.palette = std::string(value);
    return {};
}

Outcome setSize(std::string_view value, CliOptions& options)
{
    const auto x = value.find('x');
    const auto width =
        x == std::string_view::npos ? std::nullopt : parseNumber<int>(value.substr(0, x));
    const auto height =
        x == std::string_view::npos ? std::nullopt : parseNumber<int>(value.substr(x + 1));
    const auto inRange = [](int v) { return v > 0 && v <= kMaxExportDimension; };
    if (!width || !height || !inRange(*width) || !inRange(*height))
    {
        return Error(
            std::format("--size expects WIDTHxHEIGHT between 1x1 and {0}x{0} (got \"{1}\")",
                        kMaxExportDimension, value));
    }
    options.exportOptions.size = PixelSize{*width, *height};
    return {};
}

Outcome setSupersample(std::string_view value, CliOptions& options)
{
    const auto k = parseNumber<int>(value);
    if (!k || (*k != 1 && *k != 2 && *k != 4))
    {
        return Error("--supersample expects 1, 2 or 4");
    }
    options.exportOptions.supersample = *k;
    return {};
}

Outcome setHelp(std::string_view /*value*/, CliOptions& options)
{
    options.help = true;
    return {};
}

struct OptionSpec
{
    std::string_view name;
    bool             takesValue;
    Outcome (*apply)(std::string_view value, CliOptions& options);
};

constexpr std::array kOptions{
    OptionSpec{"--help", false, setHelp},
    OptionSpec{"-h", false, setHelp},
    OptionSpec{"--render", true, setRender},
    OptionSpec{"-o", true, setRender},
    OptionSpec{"--view", true, setView},
    OptionSpec{"--fractal", true, setFractal},
    OptionSpec{"--exponent", true, setExponent},
    OptionSpec{"--julia", true, setJulia},
    OptionSpec{"--center", true, setCenter},
    OptionSpec{"--zoom", true, setZoom},
    OptionSpec{"--iterations", true, setIterations},
    OptionSpec{"--palette", true, setPalette},
    OptionSpec{"--size", true, setSize},
    OptionSpec{"--supersample", true, setSupersample},
};

const OptionSpec* findOption(std::string_view name)
{
    for (const OptionSpec& option : kOptions)
    {
        if (option.name == name)
        {
            return &option;
        }
    }
    return nullptr;
}

}  // namespace

std::expected<CliOptions, std::string> parseCommandLine(std::span<const std::string_view> args)
{
    CliOptions options;
    for (std::size_t i = 0; i < args.size(); ++i)
    {
        const std::string_view arg = args[i];
        if (!arg.starts_with('-') || arg == "-")
        {
            return Error(std::format("unexpected argument \"{}\"", arg));
        }
        const auto             equals = arg.find('=');
        const std::string_view name   = arg.substr(0, equals);
        const OptionSpec*      option = findOption(name);
        if (option == nullptr)
        {
            return Error(std::format("unknown option \"{}\"", name));
        }

        std::string_view value;
        if (equals != std::string_view::npos)
        {
            if (!option->takesValue)
            {
                return Error(std::format("{} does not take a value", name));
            }
            value = arg.substr(equals + 1);
        }
        else if (option->takesValue)
        {
            if (i + 1 >= args.size())
            {
                return Error(std::format("{} needs a value", name));
            }
            value = args[++i];
        }

        if (const Outcome applied = option->apply(value, options); !applied)
        {
            return Error(applied.error());
        }
    }
    return options;
}

RenderSettings applyOverrides(RenderSettings base, const CliOverrides& overrides,
                              bool baseIsDefault)
{
    if (overrides.family)
    {
        base.fractal.family = *overrides.family;
    }
    if (overrides.exponent)
    {
        base.fractal.exponent = *overrides.exponent;
    }
    if (overrides.julia)
    {
        base.fractal.julia = true;
        base.fractal.seed  = *overrides.julia;
    }
    if (baseIsDefault)
    {
        base.view = defaultView(base.fractal);
    }
    if (overrides.zoom)
    {
        base.view.zoom = clampZoom(*overrides.zoom);
    }
    if (overrides.center)
    {
        // The zoom is final now, so the centre can take the precision it calls for.
        if (const auto center = parseCenter(*overrides.center, fractionBitsFor(base.view.zoom)))
        {
            base.view.center = *center;
        }
    }
    if (overrides.iterations)
    {
        base.maxIterations  = *overrides.iterations;
        base.autoIterations = false;
    }
    if (overrides.palette)
    {
        base.coloring.palette = *overrides.palette;
    }
    return base;
}

std::expected<RenderSettings, std::string> resolveSettings(const CliOptions& options)
{
    if (!options.viewFile)
    {
        return applyOverrides(RenderSettings{}, options.overrides, true);
    }
    try
    {
        return applyOverrides(loadView(*options.viewFile), options.overrides, false);
    }
    catch (const std::exception& e)
    {
        return Error(std::format("cannot load view {}: {}", options.viewFile->string(), e.what()));
    }
}

std::string usageText()
{
    return std::format(R"(usage: Mandelbrotter [options]

Without --render, opens the interactive window (with any view options applied).

  -h, --help              show this help and exit
  -o, --render FILE.png   render headlessly to a PNG file and exit
      --view FILE.json    start from a saved view (File > Export view..., or a bookmark)
      --fractal NAME      mandelbrot (default), burning-ship or tricorn
      --exponent N        z^N + c, N from {} to {} (default 2)
      --julia RE,IM       draw the Julia set for the constant c = RE + IM i
      --center RE,IM      centre of the view
      --zoom Z            magnification (1 shows the whole set; up to {:g})
      --iterations N      fixed iteration limit (default: automatic, grows with zoom)
      --palette NAME      one of: {}
      --size WxH          output size for --render (default 1920x1080)
      --supersample K     anti-aliasing for --render: 1, 2 or 4 samples per axis (default 1)

Options may also be written as --option=value.
)",
                       kMinExponent, kMaxExponent, kMaxZoom, paletteList());
}

int runCli(const CliOptions& options, std::ostream& out, std::ostream& err)
{
    if (options.help)
    {
        out << usageText();
        return 0;
    }
    if (!options.renderOutput)
    {
        return 0;
    }
    const auto settings = resolveSettings(options);
    if (!settings)
    {
        err << "error: " << settings.error() << '\n';
        return 1;
    }
    try
    {
        const auto                    start = std::chrono::steady_clock::now();
        const std::optional<RgbImage> image = renderForExport(*settings, options.exportOptions);
        if (!image)
        {
            err << "error: render was cancelled\n";
            return 1;
        }
        writePng(*options.renderOutput, *image);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        out << std::format("wrote {} ({}x{}, {} {}, {} iterations, {} ms)\n",
                           options.renderOutput->string(), image->width, image->height,
                           toString(settings->fractal.family),
                           settings->fractal.julia ? "julia" : "set",
                           effectiveIterations(*settings), elapsed.count());
        return 0;
    }
    catch (const std::exception& e)
    {
        err << "error: " << e.what() << '\n';
        return 1;
    }
}

}  // namespace mandelbrotter
