#include "Mandelbrotter/bookmarks.h"

#include <filesystem>
#include <fstream>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "TempDir.h"

namespace
{

using mandelbrotter::Bookmark;
using mandelbrotter::BookmarkError;
using mandelbrotter::FractalFamily;
using mandelbrotter::RenderSettings;

RenderSettings fancySettings()
{
    RenderSettings s;
    s.fractal = {
        .family = FractalFamily::BurningShip, .exponent = 3, .julia = true, .seed = {-0.8, 0.156}};
    s.view           = {{-1.7433419053, -0.0280023654}, 12345.678};
    s.maxIterations  = 4321;
    s.autoIterations = false;
    s.coloring       = {.palette = "electric", .density = 12.5, .offset = 0.31};
    return s;
}

TEST(Bookmarks, ViewJsonRoundTripsExactly)
{
    const RenderSettings original = fancySettings();
    const std::string    json     = mandelbrotter::toJson(original);
    EXPECT_NE(json.find("\"version\": 1"), std::string::npos);
    EXPECT_NE(json.find("\"burning-ship\""), std::string::npos);
    EXPECT_EQ(mandelbrotter::renderSettingsFromJson(json), original);
    EXPECT_EQ(mandelbrotter::renderSettingsFromJson(mandelbrotter::toJson(RenderSettings{})),
              RenderSettings{});
}

TEST(Bookmarks, BareSettingsObjectAndUnknownKeysAreAccepted)
{
    const auto settings = mandelbrotter::renderSettingsFromJson(R"({
        "fractal": {"family": "tricorn", "future": true},
        "view": {"center": {"re": 0.1, "im": -0.2}, "zoom": 4.0},
        "extra": [1, 2, 3]
    })");
    EXPECT_EQ(settings.fractal.family, FractalFamily::Tricorn);
    EXPECT_EQ(settings.fractal.exponent, 2);
    EXPECT_FALSE(settings.fractal.julia);
    EXPECT_EQ(settings.view.center, (mandelbrotter::Complex{0.1, -0.2}));
    EXPECT_DOUBLE_EQ(settings.view.zoom, 4.0);
    EXPECT_EQ(settings.maxIterations, mandelbrotter::kDefaultIterations);
    EXPECT_TRUE(settings.autoIterations);
    EXPECT_EQ(settings.coloring, mandelbrotter::ColoringSettings{});
}

TEST(Bookmarks, ValuesAreClampedAndUnknownPaletteFallsBack)
{
    const auto settings = mandelbrotter::renderSettingsFromJson(R"({
        "fractal": {"family": "mandelbrot", "exponent": 99},
        "view": {"center": {"re": 0, "im": 0}, "zoom": 1e300},
        "iterations": {"max": -5, "auto": false},
        "coloring": {"palette": "nope", "density": 0.001, "offset": 0.5}
    })");
    EXPECT_EQ(settings.fractal.exponent, mandelbrotter::kMaxExponent);
    EXPECT_DOUBLE_EQ(settings.view.zoom, mandelbrotter::kMaxZoom);
    EXPECT_EQ(settings.maxIterations, mandelbrotter::kMinIterations);
    EXPECT_FALSE(settings.autoIterations);
    EXPECT_EQ(settings.coloring.palette, "classic");
    EXPECT_DOUBLE_EQ(settings.coloring.density, mandelbrotter::kMinDensity);
}

TEST(Bookmarks, MalformedInputThrows)
{
    EXPECT_THROW(static_cast<void>(mandelbrotter::renderSettingsFromJson("not json")),
                 BookmarkError);
    EXPECT_THROW(static_cast<void>(mandelbrotter::renderSettingsFromJson("[1, 2]")), BookmarkError);
    EXPECT_THROW(static_cast<void>(mandelbrotter::renderSettingsFromJson(
                     R"({"fractal": {"family": "mandelbrot"}})")),
                 BookmarkError);
    EXPECT_THROW(
        static_cast<void>(mandelbrotter::renderSettingsFromJson(R"({"fractal": {"family": "cubic"},
        "view": {"center": {"re": 0, "im": 0}, "zoom": 1}})")),
        BookmarkError);
    EXPECT_THROW(static_cast<void>(
                     mandelbrotter::renderSettingsFromJson(R"({"fractal": {"family": "mandelbrot"},
        "view": {"center": {"re": "zero", "im": 0}, "zoom": 1}})")),
                 BookmarkError);
    EXPECT_THROW(static_cast<void>(
                     mandelbrotter::renderSettingsFromJson(R"({"fractal": {"family": "mandelbrot"},
        "view": {"center": {"re": 0, "im": 0}}})")),
                 BookmarkError);
    EXPECT_THROW(static_cast<void>(
                     mandelbrotter::renderSettingsFromJson(R"({"version": 99, "settings": {}})")),
                 BookmarkError);
}

TEST(Bookmarks, ListRoundTrips)
{
    const std::vector<Bookmark> original{{"Seahorse valley", fancySettings()},
                                         {"Home", RenderSettings{}}};
    const std::string           json = mandelbrotter::serializeBookmarks(original);
    EXPECT_EQ(mandelbrotter::parseBookmarks(json), original);
    EXPECT_TRUE(mandelbrotter::parseBookmarks(R"({"version": 1, "bookmarks": []})").empty());
    EXPECT_THROW(static_cast<void>(mandelbrotter::parseBookmarks(R"({"version": 1})")),
                 BookmarkError);
    EXPECT_THROW(
        static_cast<void>(mandelbrotter::parseBookmarks(R"({"version": 1, "bookmarks": {}})")),
        BookmarkError);
}

TEST(Bookmarks, FilesRoundTripAndMissingListIsEmpty)
{
    const mandelbrotter::test::TempDir dir;
    const auto                         listPath = dir / "nested" / "bookmarks.json";
    EXPECT_TRUE(mandelbrotter::loadBookmarks(listPath).empty());

    const std::vector<Bookmark> bookmarks{{"One", fancySettings()}};
    mandelbrotter::saveBookmarks(listPath, bookmarks);
    EXPECT_EQ(mandelbrotter::loadBookmarks(listPath), bookmarks);
    EXPECT_FALSE(std::filesystem::exists(listPath.string() + ".tmp"));

    const auto viewPath = dir / "view.json";
    mandelbrotter::saveView(viewPath, fancySettings());
    EXPECT_EQ(mandelbrotter::loadView(viewPath), fancySettings());
    EXPECT_THROW(static_cast<void>(mandelbrotter::loadView(dir / "missing.json")), BookmarkError);

    std::ofstream(listPath) << "garbage";
    EXPECT_THROW(static_cast<void>(mandelbrotter::loadBookmarks(listPath)), BookmarkError);
}

}  // namespace
