#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include "Mandelbrotter/RenderSettings.h"

namespace mandelbrotter::app
{

/// What main() hands to the window: the view to open with and, for the developer screenshot mode
/// (--screenshots DIR), where to write the help book's pictures.
struct StartupOptions
{
    RenderSettings                       settings;
    std::optional<std::filesystem::path> screenshotsDir;
};

/// The screenshot mode's scratch directory under the temp directory. The screenshot run deletes
/// the bookmarks file's directory when it ends, so it must only ever receive this path.
inline constexpr std::string_view kScratchDirName = "Mandelbrotter-screenshots";

/// Creates `tempDir`/kScratchDirName/bookmarks.json holding screenshotBookmarks() and returns its
/// path: the screenshot mode shows a few bookmarks without touching the user's file. Throws on
/// write errors.
[[nodiscard]] std::filesystem::path scratchBookmarksPath(const std::filesystem::path& tempDir);

/// The bookmarks file the window uses: `userDataDir`/bookmarks.json, or the scratch file in
/// screenshot mode.
[[nodiscard]] std::filesystem::path bookmarksPathFor(const StartupOptions&        options,
                                                     const std::filesystem::path& userDataDir,
                                                     const std::filesystem::path& tempDir);

}  // namespace mandelbrotter::app
