#include "Mandelbrotter/bookmarks.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <ios>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter
{

namespace
{

using nlohmann::json;

constexpr int kSchemaVersion = 1;

json complexToJson(Complex c)
{
    return {{"re", c.re}, {"im", c.im}};
}

json settingsToJson(const RenderSettings& s)
{
    return {{"fractal",
             {{"family", toString(s.fractal.family)},
              {"exponent", s.fractal.exponent},
              {"julia", s.fractal.julia},
              {"seed", complexToJson(s.fractal.seed)}}},
            {"view", {{"center", complexToJson(s.view.center)}, {"zoom", s.view.zoom}}},
            {"iterations", {{"max", s.maxIterations}, {"auto", s.autoIterations}}},
            {"coloring",
             {{"palette", s.coloring.palette},
              {"density", s.coloring.density},
              {"offset", s.coloring.offset}}}};
}

const json& require(const json& object, const char* key)
{
    if (!object.is_object() || !object.contains(key))
    {
        throw BookmarkError(std::string("missing \"") + key + "\"");
    }
    return object.at(key);
}

template <typename T>
T get(const json& object, const char* key, const T& fallback)
{
    if (!object.is_object() || !object.contains(key))
    {
        return fallback;
    }
    try
    {
        return object.at(key).get<T>();
    }
    catch (const json::exception& e)
    {
        throw BookmarkError(std::string("bad value for \"") + key + "\": " + e.what());
    }
}

template <typename T>
T getRequired(const json& object, const char* key)
{
    const json& value = require(object, key);
    try
    {
        return value.get<T>();
    }
    catch (const json::exception& e)
    {
        throw BookmarkError(std::string("bad value for \"") + key + "\": " + e.what());
    }
}

double finite(double value, const char* key)
{
    if (!std::isfinite(value))
    {
        throw BookmarkError(std::string("\"") + key + "\" is not a finite number");
    }
    return value;
}

Complex complexFromJson(const json& object, const char* key)
{
    const json& c = require(object, key);
    return {finite(get<double>(c, "re", 0.0), key), finite(get<double>(c, "im", 0.0), key)};
}

RenderSettings settingsFromJson(const json& s)
{
    if (!s.is_object())
    {
        throw BookmarkError("settings must be an object");
    }
    RenderSettings out;

    const json& fractal      = require(s, "fractal");
    const auto  family       = get<std::string>(fractal, "family", "mandelbrot");
    const auto  parsedFamily = parseFamily(family);
    if (!parsedFamily)
    {
        throw BookmarkError("unknown fractal family \"" + family + "\"");
    }
    out.fractal.family   = *parsedFamily;
    out.fractal.exponent = clampExponent(get<int>(fractal, "exponent", 2));
    out.fractal.julia    = get<bool>(fractal, "julia", false);
    if (fractal.contains("seed"))
    {
        out.fractal.seed = complexFromJson(fractal, "seed");
    }

    const json& view = require(s, "view");
    out.view.center  = complexFromJson(view, "center");
    out.view.zoom    = clampZoom(finite(getRequired<double>(view, "zoom"), "zoom"));

    if (s.contains("iterations"))
    {
        const json& iterations = s.at("iterations");
        out.maxIterations      = std::clamp(get<int>(iterations, "max", kDefaultIterations),
                                            kMinIterations, kMaxIterations);
        out.autoIterations     = get<bool>(iterations, "auto", true);
    }

    if (s.contains("coloring"))
    {
        const json& coloring = s.at("coloring");
        out.coloring.palette =
            paletteOrDefault(get<std::string>(coloring, "palette", "classic")).name();
        out.coloring.density = std::clamp(finite(get<double>(coloring, "density", 64.0), "density"),
                                          kMinDensity, kMaxDensity);
        out.coloring.offset  = finite(get<double>(coloring, "offset", 0.0), "offset");
    }
    return out;
}

json parse(std::string_view text)
{
    try
    {
        return json::parse(text);
    }
    catch (const json::exception& e)
    {
        throw BookmarkError(std::string("invalid JSON: ") + e.what());
    }
}

void checkVersion(const json& document)
{
    const int version = get<int>(document, "version", kSchemaVersion);
    if (version > kSchemaVersion)
    {
        throw BookmarkError("file was written by a newer version of Mandelbrotter");
    }
}

std::string readFile(const std::filesystem::path& path)
{
    const std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        throw BookmarkError("cannot read " + path.string());
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void writeFileAtomically(const std::filesystem::path& path, std::string_view contents)
{
    std::error_code ec;
    if (path.has_parent_path())
    {
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    const std::filesystem::path temporary = path.string() + ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            throw BookmarkError("cannot write " + temporary.string());
        }
        file << contents;
        if (!file)
        {
            throw BookmarkError("failed to write " + temporary.string());
        }
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec)
    {
        std::filesystem::remove(temporary, ec);
        throw BookmarkError("cannot replace " + path.string());
    }
}

}  // namespace

std::string toJson(const RenderSettings& settings)
{
    const json document{{"version", kSchemaVersion}, {"settings", settingsToJson(settings)}};
    return document.dump(2) + "\n";
}

RenderSettings renderSettingsFromJson(std::string_view text)
{
    const json document = parse(text);
    if (document.is_object() && document.contains("settings"))
    {
        checkVersion(document);
        return settingsFromJson(document.at("settings"));
    }
    return settingsFromJson(document);
}

std::string serializeBookmarks(std::span<const Bookmark> bookmarks)
{
    json list = json::array();
    for (const Bookmark& bookmark : bookmarks)
    {
        list.push_back({{"name", bookmark.name}, {"settings", settingsToJson(bookmark.settings)}});
    }
    const json document{{"version", kSchemaVersion}, {"bookmarks", std::move(list)}};
    return document.dump(2) + "\n";
}

std::vector<Bookmark> parseBookmarks(std::string_view text)
{
    const json document = parse(text);
    checkVersion(document);
    const json& list = require(document, "bookmarks");
    if (!list.is_array())
    {
        throw BookmarkError("\"bookmarks\" must be an array");
    }
    std::vector<Bookmark> bookmarks;
    for (const json& entry : list)
    {
        bookmarks.push_back({.name     = get<std::string>(entry, "name", "Untitled"),
                             .settings = settingsFromJson(require(entry, "settings"))});
    }
    return bookmarks;
}

std::vector<Bookmark> loadBookmarks(const std::filesystem::path& path)
{
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
    {
        return {};
    }
    return parseBookmarks(readFile(path));
}

void saveBookmarks(const std::filesystem::path& path, std::span<const Bookmark> bookmarks)
{
    writeFileAtomically(path, serializeBookmarks(bookmarks));
}

RenderSettings loadView(const std::filesystem::path& path)
{
    return renderSettingsFromJson(readFile(path));
}

void saveView(const std::filesystem::path& path, const RenderSettings& settings)
{
    writeFileAtomically(path, toJson(settings));
}

}  // namespace mandelbrotter
