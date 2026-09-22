#pragma once

#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "Mandelbrotter/render_settings.h"

namespace mandelbrotter
{

/// A named view. Bookmarks and view files share one JSON schema (see toJson).
struct Bookmark
{
    std::string    name;
    RenderSettings settings;

    bool operator==(const Bookmark&) const = default;
};

class BookmarkError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

/// A view file: {"version": 1, "settings": {fractal, view, iterations, coloring}}.
[[nodiscard]] std::string toJson(const RenderSettings& settings);
/// Parses a view file, or a bare settings object. Unknown keys are ignored; out-of-range values are
/// clamped; an unknown palette falls back to the default. Throws BookmarkError for malformed input.
[[nodiscard]] RenderSettings renderSettingsFromJson(std::string_view text);

/// A bookmarks file: {"version": 1, "bookmarks": [{"name": ..., "settings": {...}}, ...]}.
[[nodiscard]] std::string           serializeBookmarks(std::span<const Bookmark> bookmarks);
[[nodiscard]] std::vector<Bookmark> parseBookmarks(std::string_view text);

/// File helpers. A missing bookmarks file is an empty list; other errors throw BookmarkError.
/// Writes go to a temporary file first and are then renamed into place; parent directories are
/// created.
[[nodiscard]] std::vector<Bookmark> loadBookmarks(const std::filesystem::path& path);
void saveBookmarks(const std::filesystem::path& path, std::span<const Bookmark> bookmarks);
[[nodiscard]] RenderSettings loadView(const std::filesystem::path& path);
void saveView(const std::filesystem::path& path, const RenderSettings& settings);

}  // namespace mandelbrotter
