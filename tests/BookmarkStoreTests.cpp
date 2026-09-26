#include "Mandelbrotter/app/BookmarkStore.h"

#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/fractal.h"
#include "TempDir.h"

namespace
{

using mandelbrotter::Bookmark;
using mandelbrotter::RenderSettings;
using mandelbrotter::app::BookmarkStore;
using mandelbrotter::test::TempDir;

Bookmark named(const std::string& name, int iterations)
{
    RenderSettings settings;
    settings.maxIterations = iterations;
    return {.name = name, .settings = settings};
}

TEST(BookmarkStore, MissingFileIsAnEmptyListAndNoError)
{
    const TempDir dir;
    BookmarkStore store(dir / "none" / "bookmarks.json");
    EXPECT_EQ(store.load(), "");
    EXPECT_TRUE(store.bookmarks().empty());
    EXPECT_EQ(store.path(), dir / "none" / "bookmarks.json");
}

TEST(BookmarkStore, SavedBookmarksLoadBackInOrder)
{
    const TempDir dir;
    BookmarkStore store(dir / "sub" / "bookmarks.json");
    store.add(named("first", 100));
    Bookmark second                = named("second", 200);
    second.settings.fractal.family = mandelbrotter::FractalFamily::TRICORN;
    store.add(second);
    EXPECT_EQ(store.save(), "");

    BookmarkStore reloaded(dir / "sub" / "bookmarks.json");
    EXPECT_EQ(reloaded.load(), "");
    ASSERT_EQ(reloaded.bookmarks().size(), 2U);
    EXPECT_EQ(reloaded.bookmarks()[0], named("first", 100));
    EXPECT_EQ(reloaded.bookmarks()[1], second);
}

TEST(BookmarkStore, GarbageFileGivesAnErrorMessage)
{
    const TempDir dir;
    std::ofstream(dir / "bookmarks.json") << "{ not json";
    BookmarkStore store(dir / "bookmarks.json");
    EXPECT_NE(store.load(), "");
    EXPECT_TRUE(store.bookmarks().empty());
}

TEST(BookmarkStore, UnwritablePathGivesAnErrorMessage)
{
    const TempDir dir;
    std::ofstream(dir / "file") << "a file, not a directory";
    BookmarkStore store(dir / "file" / "bookmarks.json");
    store.add(named("x", 1));
    EXPECT_NE(store.save(), "");
}

TEST(BookmarkStore, RemoveDeletesOneEntryAndIgnoresBadIndices)
{
    const TempDir dir;
    BookmarkStore store(dir / "bookmarks.json");
    store.add(named("a", 1));
    store.add(named("b", 2));
    store.add(named("c", 3));
    store.remove(1);
    ASSERT_EQ(store.bookmarks().size(), 2U);
    EXPECT_EQ(store.bookmarks()[0].name, "a");
    EXPECT_EQ(store.bookmarks()[1].name, "c");
    store.remove(2);
    store.remove(99);
    EXPECT_EQ(store.bookmarks().size(), 2U);
    EXPECT_FALSE(std::filesystem::exists(dir / "bookmarks.json"));  // only save() writes
}

}  // namespace
