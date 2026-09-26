#include "Mandelbrotter/app/startup.h"

#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "Mandelbrotter/app/scenes.h"
#include "Mandelbrotter/bookmarks.h"
#include "TempDir.h"

namespace
{

using mandelbrotter::app::StartupOptions;
using mandelbrotter::test::TempDir;
namespace app = mandelbrotter::app;

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
