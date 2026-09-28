#pragma once

#include <filesystem>
#include <optional>
#include <ostream>
#include <span>
#include <string_view>
#include <variant>

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

/// The command line, parsed and acted on as far as it can be without a window: an exit code when
/// the program is done (--help, --render, --flight-frames, or an error, reported on `err`), or the
/// options for the window. Drops macOS's "-psn_..." argument. `args` are the arguments after the
/// program name.
[[nodiscard]] std::variant<int, StartupOptions> prepareStartup(
    std::span<const std::string_view> args, std::ostream& out, std::ostream& err);

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
