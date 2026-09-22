#include "gui/bookmark_store.h"

#include <exception>
#include <utility>

#include <wx/stdpaths.h>

#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

BookmarkStore::BookmarkStore()
  : m_path(std::filesystem::path(fromWx(wxStandardPaths::Get().GetUserDataDir())) /
           "bookmarks.json")
{
}

BookmarkStore::BookmarkStore(std::filesystem::path path) : m_path(std::move(path)) { }

std::string BookmarkStore::load()
{
    try
    {
        m_bookmarks = loadBookmarks(m_path);
        return {};
    }
    catch (const std::exception& e)
    {
        return e.what();
    }
}

std::string BookmarkStore::save() const
{
    try
    {
        saveBookmarks(m_path, m_bookmarks);
        return {};
    }
    catch (const std::exception& e)
    {
        return e.what();
    }
}

void BookmarkStore::add(Bookmark bookmark)
{
    m_bookmarks.push_back(std::move(bookmark));
}

void BookmarkStore::remove(std::size_t index)
{
    if (index < m_bookmarks.size())
    {
        m_bookmarks.erase(m_bookmarks.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

}  // namespace mandelbrotter::gui
