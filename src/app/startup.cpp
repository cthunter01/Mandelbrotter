#include "Mandelbrotter/app/startup.h"

#include <filesystem>
#include <format>
#include <ostream>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "Mandelbrotter/app/scenes.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/cli.h"

namespace mandelbrotter::app
{

namespace
{

constexpr const char* kBookmarksFile = "bookmarks.json";

}  // namespace

std::variant<int, StartupOptions> prepareStartup(std::span<const std::string_view> args,
                                                 std::ostream& out, std::ostream& err)
{
    std::vector<std::string_view> ours(args.begin(), args.end());
    // macOS has passed "-psn_<process serial number>" to an app bundle started from Finder; it is
    // not ours.
    std::erase_if(ours, [](std::string_view arg) { return arg.starts_with("-psn_"); });

    const auto options = parseCommandLine(ours);
    if (!options)
    {
        err << std::format("error: {}\n\n{}\n", options.error(), usageText());
        return 2;
    }
    if (!options->wantsGui())
    {
        return runCli(*options, out, err);
    }
    const auto settings = resolveSettings(*options);
    if (!settings)
    {
        err << std::format("error: {}\n", settings.error());
        return 2;
    }
    return StartupOptions{.settings = *settings, .screenshotsDir = options->screenshotsDir};
}

std::filesystem::path scratchBookmarksPath(const std::filesystem::path& tempDir)
{
    const std::filesystem::path dir = tempDir / kScratchDirName;
    std::filesystem::create_directories(dir);
    const std::filesystem::path path = dir / kBookmarksFile;
    saveBookmarks(path, screenshotBookmarks());
    return path;
}

std::filesystem::path bookmarksPathFor(const StartupOptions&        options,
                                       const std::filesystem::path& userDataDir,
                                       const std::filesystem::path& tempDir)
{
    if (options.screenshotsDir)
    {
        return scratchBookmarksPath(tempDir);
    }
    return userDataDir / kBookmarksFile;
}

}  // namespace mandelbrotter::app
