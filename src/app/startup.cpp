#include "Mandelbrotter/app/startup.h"

#include <filesystem>

#include "Mandelbrotter/app/scenes.h"
#include "Mandelbrotter/bookmarks.h"

namespace mandelbrotter::app
{

namespace
{

constexpr const char* kBookmarksFile = "bookmarks.json";

}  // namespace

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
