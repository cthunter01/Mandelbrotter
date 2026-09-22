#include "Mandelbrotter/cli.h"

#include <expected>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/exporter.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/palette.h"
#include "Mandelbrotter/render_settings.h"
#include "test_util.h"

namespace
{

using mandelbrotter::CliOptions;
using mandelbrotter::Complex;
using mandelbrotter::FractalFamily;
using mandelbrotter::RenderSettings;

std::expected<CliOptions, std::string> parse(std::vector<std::string_view> args)
{
    return mandelbrotter::parseCommandLine(args);
}

TEST(Cli, NoArgumentsOpensTheGui)
{
    const auto options = parse({});
    ASSERT_TRUE(options.has_value());
    EXPECT_TRUE(options->wantsGui());
    EXPECT_FALSE(options->help);
    EXPECT_EQ(options->exportOptions, mandelbrotter::ExportOptions{});
}

TEST(Cli, HelpAndRenderDoNotOpenTheGui)
{
    for (const std::string_view flag : {"--help", "-h"})
    {
        const auto options = parse({flag});
        ASSERT_TRUE(options.has_value()) << flag;
        EXPECT_TRUE(options->help);
        EXPECT_FALSE(options->wantsGui());
    }
    const auto render = parse({"--render", "out.png"});
    ASSERT_TRUE(render.has_value());
    EXPECT_FALSE(render->wantsGui());
    EXPECT_EQ(render->renderOutput, std::filesystem::path("out.png"));
}

TEST(Cli, EveryOptionParsesInBothSpellings)
{
    const auto spaced =
        parse({"--render",   "a.png",   "--view",        "v.json",     "--fractal", "burning-ship",
               "--exponent", "3",       "--julia",       "-0.8,0.156", "--center",  "-0.75,0.1",
               "--zoom",     "5e3",     "--iterations",  "1000",       "--palette", "fire",
               "--size",     "800x600", "--supersample", "4"});
    const auto equals =
        parse({"--render=a.png", "--view=v.json", "--fractal=burning-ship", "--exponent=3",
               "--julia=-0.8,0.156", "--center=-0.75,0.1", "--zoom=5e3", "--iterations=1000",
               "--palette=fire", "--size=800x600", "--supersample=4"});
    ASSERT_TRUE(spaced.has_value()) << spaced.error();
    ASSERT_TRUE(equals.has_value()) << equals.error();
    EXPECT_EQ(spaced->renderOutput, std::filesystem::path("a.png"));
    EXPECT_EQ(spaced->viewFile, std::filesystem::path("v.json"));
    EXPECT_EQ(spaced->overrides.family, FractalFamily::BurningShip);
    EXPECT_EQ(spaced->overrides.exponent, 3);
    EXPECT_EQ(spaced->overrides.julia, (Complex{-0.8, 0.156}));
    EXPECT_EQ(spaced->overrides.center, (Complex{-0.75, 0.1}));
    EXPECT_EQ(spaced->overrides.zoom, 5000.0);
    EXPECT_EQ(spaced->overrides.iterations, 1000);
    EXPECT_EQ(spaced->overrides.palette, "fire");
    EXPECT_EQ(spaced->exportOptions.size, (mandelbrotter::PixelSize{800, 600}));
    EXPECT_EQ(spaced->exportOptions.supersample, 4);
    EXPECT_EQ(spaced->overrides, equals->overrides);
    EXPECT_EQ(spaced->exportOptions, equals->exportOptions);
    EXPECT_EQ(spaced->renderOutput, equals->renderOutput);
    EXPECT_EQ(spaced->viewFile, equals->viewFile);
}

TEST(Cli, BadValuesAreRejectedWithHelpfulMessages)
{
    const std::vector<std::pair<std::vector<std::string_view>, std::string>> cases{
        {{"--bogus"}, "unknown option"},
        {{"stray"}, "unexpected argument"},
        {{"--zoom"}, "needs a value"},
        {{"--help=yes"}, "does not take a value"},
        {{"--fractal", "cubic"}, "unknown fractal"},
        {{"--exponent", "9"}, "--exponent expects"},
        {{"--exponent", "two"}, "--exponent expects"},
        {{"--julia", "1"}, "re,im"},
        {{"--center", "a,b"}, "re,im"},
        {{"--zoom", "-1"}, "positive"},
        {{"--zoom", "nan"}, "positive"},
        {{"--iterations", "0"}, "--iterations expects"},
        {{"--palette", "nope"}, "classic"},
        {{"--size", "800"}, "WIDTHxHEIGHT"},
        {{"--size", "0x10"}, "WIDTHxHEIGHT"},
        {{"--size", "99999x10"}, "WIDTHxHEIGHT"},
        {{"--supersample", "3"}, "1, 2 or 4"},
    };
    for (const auto& [args, expected] : cases)
    {
        const auto options = parse(args);
        ASSERT_FALSE(options.has_value()) << args.front();
        EXPECT_NE(options.error().find(expected), std::string::npos) << options.error();
    }
}

TEST(Cli, OverridesStartFromTheFamilyDefaultView)
{
    const auto options = parse({"--fractal", "burning-ship", "--iterations", "50"});
    ASSERT_TRUE(options.has_value());
    const auto settings = mandelbrotter::resolveSettings(*options);
    ASSERT_TRUE(settings.has_value());
    EXPECT_EQ(settings->fractal.family, FractalFamily::BurningShip);
    EXPECT_EQ(settings->view, mandelbrotter::defaultView(settings->fractal));
    EXPECT_EQ(settings->maxIterations, 50);
    EXPECT_FALSE(settings->autoIterations);

    const auto julia = parse({"--julia", "0.3,0.5", "--zoom", "2", "--palette", "ocean"});
    ASSERT_TRUE(julia.has_value());
    const auto juliaSettings = mandelbrotter::resolveSettings(*julia);
    ASSERT_TRUE(juliaSettings.has_value());
    EXPECT_TRUE(juliaSettings->fractal.julia);
    EXPECT_EQ(juliaSettings->fractal.seed, (Complex{0.3, 0.5}));
    EXPECT_EQ(juliaSettings->view.center,
              mandelbrotter::defaultView(juliaSettings->fractal).center);
    EXPECT_DOUBLE_EQ(juliaSettings->view.zoom, 2.0);
    EXPECT_EQ(juliaSettings->coloring.palette, "ocean");
    EXPECT_TRUE(juliaSettings->autoIterations);
}

TEST(Cli, ViewFileIsLoadedThenOverridden)
{
    const mandelbrotter::test::TempDir dir;
    RenderSettings                     saved;
    saved.fractal.family   = FractalFamily::Tricorn;
    saved.view             = {{0.3, 0.4}, 77.0};
    saved.maxIterations    = 500;
    saved.autoIterations   = false;
    saved.coloring.palette = "rainbow";
    const auto path        = dir / "view.json";
    mandelbrotter::saveView(path, saved);

    const auto options = parse({"--zoom", "10", "--view", path.string(), "--palette", "grayscale"});
    ASSERT_TRUE(options.has_value());
    const auto settings = mandelbrotter::resolveSettings(*options);
    ASSERT_TRUE(settings.has_value()) << settings.error();
    EXPECT_EQ(settings->fractal.family, FractalFamily::Tricorn);
    EXPECT_EQ(settings->view.center, (Complex{0.3, 0.4}));
    EXPECT_DOUBLE_EQ(settings->view.zoom, 10.0);
    EXPECT_EQ(settings->maxIterations, 500);
    EXPECT_EQ(settings->coloring.palette, "grayscale");

    const auto missing = parse({"--view", (dir / "missing.json").string()});
    ASSERT_TRUE(missing.has_value());
    const auto failed = mandelbrotter::resolveSettings(*missing);
    ASSERT_FALSE(failed.has_value());
    EXPECT_NE(failed.error().find("cannot load view"), std::string::npos);
}

TEST(Cli, UsageMentionsEveryOptionAndPalette)
{
    const std::string usage = mandelbrotter::usageText();
    for (const std::string_view option :
         {"--help", "--render", "--view", "--fractal", "--exponent", "--julia", "--center",
          "--zoom", "--iterations", "--palette", "--size", "--supersample"})
    {
        EXPECT_NE(usage.find(option), std::string::npos) << option;
    }
    for (const auto name : mandelbrotter::paletteNames())
    {
        EXPECT_NE(usage.find(name), std::string::npos) << name;
    }
}

TEST(Cli, RunPrintsHelpOrRendersAPng)
{
    std::ostringstream out;
    std::ostringstream err;
    const auto         help = parse({"--help"});
    ASSERT_TRUE(help.has_value());
    EXPECT_EQ(mandelbrotter::runCli(*help, out, err), 0);
    EXPECT_EQ(out.str(), mandelbrotter::usageText());

    const mandelbrotter::test::TempDir dir;
    const auto                         path   = dir / "render.png";
    const auto                         render = parse(
        {"--render", path.string(), "--size", "32x24", "--iterations", "40", "--supersample", "2"});
    ASSERT_TRUE(render.has_value());
    out.str("");
    EXPECT_EQ(mandelbrotter::runCli(*render, out, err), 0) << err.str();
    EXPECT_TRUE(std::filesystem::exists(path));
    EXPECT_GT(std::filesystem::file_size(path), 100U);
    EXPECT_NE(out.str().find("32x24"), std::string::npos);
    EXPECT_NE(out.str().find("40 iterations"), std::string::npos);

    const auto unwritable =
        parse({"--render", (dir / "missing" / "x.png").string(), "--size", "4x4"});
    ASSERT_TRUE(unwritable.has_value());
    EXPECT_EQ(mandelbrotter::runCli(*unwritable, out, err), 1);
    EXPECT_NE(err.str().find("error:"), std::string::npos);

    const auto badView = parse({"--render", path.string(), "--view", (dir / "none.json").string()});
    ASSERT_TRUE(badView.has_value());
    err.str("");
    EXPECT_EQ(mandelbrotter::runCli(*badView, out, err), 1);
    EXPECT_NE(err.str().find("cannot load view"), std::string::npos);
}

}  // namespace
