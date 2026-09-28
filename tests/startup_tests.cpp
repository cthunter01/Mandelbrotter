#include "Mandelbrotter/app/startup.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/app/scenes.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/cli.h"
#include "Mandelbrotter/fractal.h"
#include "TempDir.h"

namespace
{

using mandelbrotter::app::StartupOptions;
using mandelbrotter::test::TempDir;
namespace app = mandelbrotter::app;

/// prepareStartup() with its output captured.
struct Prepared
{
    std::variant<int, StartupOptions> result;
    std::string                       out;
    std::string                       err;
};

Prepared prepare(const std::vector<std::string_view>& args)
{
    std::ostringstream out;
    std::ostringstream err;
    auto               result = app::prepareStartup(args, out, err);
    return {.result = std::move(result), .out = out.str(), .err = err.str()};
}

TEST(Startup, HelpPrintsTheUsageAndEnds)
{
    const Prepared prepared = prepare({"--help"});
    ASSERT_TRUE(std::holds_alternative<int>(prepared.result));
    EXPECT_EQ(std::get<int>(prepared.result), 0);
    EXPECT_EQ(prepared.out, mandelbrotter::usageText());
    EXPECT_TRUE(prepared.err.empty());
}

TEST(Startup, AnUnknownOptionIsReportedWithTheUsage)
{
    const Prepared prepared = prepare({"--bogus"});
    ASSERT_TRUE(std::holds_alternative<int>(prepared.result));
    EXPECT_EQ(std::get<int>(prepared.result), 2);
    EXPECT_TRUE(prepared.err.starts_with("error: unknown option \"--bogus\"\n\n")) << prepared.err;
    EXPECT_TRUE(prepared.err.contains(mandelbrotter::usageText()));
    EXPECT_TRUE(prepared.out.empty());
}

TEST(Startup, RenderWritesAPictureWithoutAWindow)
{
    const TempDir     temp;
    const std::string file  = (temp / "a.png").string();
    const Prepared prepared = prepare({"--render", file, "--size", "32x20", "--iterations", "50"});
    ASSERT_TRUE(std::holds_alternative<int>(prepared.result));
    EXPECT_EQ(std::get<int>(prepared.result), 0) << prepared.err;
    EXPECT_TRUE(prepared.out.contains("32x20")) << prepared.out;

    std::ifstream       png(file, std::ios::binary);
    std::array<char, 8> signature{};
    png.read(signature.data(), signature.size());
    EXPECT_EQ(std::string_view(signature.data(), signature.size()), "\x89PNG\r\n\x1a\n");
}

TEST(Startup, TheMacOSProcessSerialNumberIsIgnored)
{
    const Prepared prepared = prepare({"-psn_0_123", "--help"});
    ASSERT_TRUE(std::holds_alternative<int>(prepared.result));
    EXPECT_EQ(std::get<int>(prepared.result), 0);
    EXPECT_TRUE(std::holds_alternative<StartupOptions>(prepare({"-psn_0_123"}).result));
}

TEST(Startup, WindowArgumentsBecomeTheWindowsOptions)
{
    const Prepared prepared =
        prepare({"--fractal", "tricorn", "--iterations", "500", "--screenshots", "shots"});
    ASSERT_TRUE(std::holds_alternative<StartupOptions>(prepared.result)) << prepared.err;
    const auto& options = std::get<StartupOptions>(prepared.result);
    EXPECT_EQ(options.settings.fractal.family, mandelbrotter::FractalFamily::TRICORN);
    EXPECT_EQ(options.settings.maxIterations, 500);
    EXPECT_FALSE(options.settings.autoIterations);
    EXPECT_EQ(options.screenshotsDir, std::filesystem::path("shots"));
    EXPECT_TRUE(prepared.out.empty());
    EXPECT_TRUE(prepared.err.empty());

    const Prepared plain = prepare({});
    ASSERT_TRUE(std::holds_alternative<StartupOptions>(plain.result));
    EXPECT_FALSE(std::get<StartupOptions>(plain.result).screenshotsDir);
}

TEST(Startup, AViewFileTheWindowCannotReadIsAnError)
{
    const TempDir  temp;
    const Prepared prepared = prepare({"--view", (temp / "missing.json").string()});
    ASSERT_TRUE(std::holds_alternative<int>(prepared.result));
    EXPECT_EQ(std::get<int>(prepared.result), 2);
    EXPECT_TRUE(prepared.err.starts_with("error: cannot load view")) << prepared.err;
}

TEST(Startup, ScratchBookmarksAreCreatedUnderTheTempDirectory)
{
    const TempDir               temp;
    const std::filesystem::path path = app::scratchBookmarksPath(temp.path());
    EXPECT_EQ(path, temp / std::string(app::kScratchDirName) / "bookmarks.json");
    ASSERT_TRUE(std::filesystem::is_regular_file(path));
    EXPECT_EQ(mandelbrotter::loadBookmarks(path), app::screenshotBookmarks());
}

TEST(Startup, ScratchBookmarksOverwriteAnEarlierRun)
{
    const TempDir temp;
    std::filesystem::create_directories(temp / std::string(app::kScratchDirName));
    mandelbrotter::saveBookmarks(temp / std::string(app::kScratchDirName) / "bookmarks.json", {});
    EXPECT_EQ(mandelbrotter::loadBookmarks(app::scratchBookmarksPath(temp.path())),
              app::screenshotBookmarks());
}

TEST(Startup, BookmarksLiveInTheUserDataDirectory)
{
    const TempDir        temp;
    const StartupOptions options;
    EXPECT_EQ(app::bookmarksPathFor(options, temp / "user", temp / "tmp"),
              temp / "user" / "bookmarks.json");
    EXPECT_FALSE(std::filesystem::exists(temp / "tmp"));  // nothing is created
    EXPECT_FALSE(std::filesystem::exists(temp / "user"));
}

TEST(Startup, ScreenshotModeUsesTheScratchBookmarks)
{
    const TempDir  temp;
    StartupOptions options;
    options.screenshotsDir           = temp / "shots";
    const std::filesystem::path path = app::bookmarksPathFor(options, temp / "user", temp.path());
    EXPECT_EQ(path, app::scratchBookmarksPath(temp.path()));
    EXPECT_TRUE(std::filesystem::is_regular_file(path));
    EXPECT_FALSE(std::filesystem::exists(temp / "user"));
    EXPECT_FALSE(std::filesystem::exists(temp / "shots"));  // the screenshot run creates it
}

}  // namespace
