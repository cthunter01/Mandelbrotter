#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "Mandelbrotter/bookmarks.h"

namespace mandelbrotter::gui
{

/// The user's bookmark list, persisted as JSON in the per-user data directory.
class BookmarkStore
{
public:
    /// Uses <user data dir>/bookmarks.json.
    BookmarkStore();
    explicit BookmarkStore(std::filesystem::path path);

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }
    [[nodiscard]] const std::vector<Bookmark>& bookmarks() const noexcept { return m_bookmarks; }

    /// Reads the file. Returns an error message, or empty on success (a missing file is not an
    /// error).
    [[nodiscard]] std::string load();
    /// Writes the file. Returns an error message, or empty on success.
    [[nodiscard]] std::string save() const;

    void add(Bookmark bookmark);
    void remove(std::size_t index);

private:
    std::filesystem::path m_path;
    std::vector<Bookmark> m_bookmarks;
};

}  // namespace mandelbrotter::gui
