// The help book in docs/help (MANDELBROTTER_HELP_DIR): control files, pages, links, images and
// "mandelbrotter:" actions must all agree with each other and with the program.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/app/help_sitemap.h"
#include "Mandelbrotter/flights.h"
#include "Mandelbrotter/help_action.h"
#include "Mandelbrotter/help_images.h"

namespace
{

constexpr std::uintmax_t kImageBudgetBytes = std::uintmax_t{5} * 1024 * 1024;

std::filesystem::path helpDir()
{
    return std::filesystem::path{MANDELBROTTER_HELP_DIR};
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<std::filesystem::path> pages()
{
    std::vector<std::filesystem::path> result;
    for (const auto& entry : std::filesystem::directory_iterator(helpDir()))
    {
        if (entry.path().extension() == ".html")
        {
            result.push_back(entry.path());
        }
    }
    std::ranges::sort(result);
    return result;
}

/// The files of an images directory: images/ (the rendered examples and the wx build's screenshots)
/// or images-qt/ (the Qt build's screenshots; empty when there is none).
std::vector<std::filesystem::path> imageFiles(std::string_view dir = "images")
{
    std::vector<std::filesystem::path> result;
    if (!std::filesystem::is_directory(helpDir() / dir))
    {
        return result;
    }
    for (const auto& entry : std::filesystem::directory_iterator(helpDir() / dir))
    {
        if (entry.is_regular_file())
        {
            result.push_back(entry.path());
        }
    }
    std::ranges::sort(result);
    return result;
}

bool isScreenshot(const std::filesystem::path& file)
{
    return file.filename().string().starts_with("ui-");
}

/// The values of every `attribute="..."` in `html`.
std::vector<std::string> attributeValues(const std::string& html, const std::string& attribute)
{
    const std::regex pattern("\\b" + attribute + R"re(\s*=\s*"([^"]*)")re", std::regex::icase);
    std::vector<std::string> values;
    for (std::sregex_iterator it(html.begin(), html.end(), pattern), end; it != end; ++it)
    {
        values.push_back((*it)[1].str());
    }
    return values;
}

void flatten(const std::vector<mandelbrotter::app::HelpTopic>& topics,
             std::vector<std::pair<std::string, std::string>>& into)
{
    for (const mandelbrotter::app::HelpTopic& topic : topics)
    {
        into.emplace_back(topic.name, topic.local);
        flatten(topic.children, into);
    }
}

/// The (Name, Local) pairs of a sitemap file (.hhc / .hhk), nested entries included.
std::vector<std::pair<std::string, std::string>> sitemapEntries(const std::string& sitemap)
{
    std::vector<std::pair<std::string, std::string>> entries;
    flatten(mandelbrotter::app::parseHelpSitemap(sitemap), entries);
    return entries;
}

/// True when `target` ("page.html" or "page.html#anchor") names an existing page and anchor.
testing::AssertionResult pageExists(const std::string& target)
{
    const auto        hash   = target.find('#');
    const std::string file   = target.substr(0, hash);
    const std::string anchor = hash == std::string::npos ? "" : target.substr(hash + 1);
    if (file.empty() || !std::filesystem::exists(helpDir() / file))
    {
        return testing::AssertionFailure() << "no page " << file;
    }
    if (!anchor.empty())
    {
        const std::string html = readFile(helpDir() / file);
        if (!html.contains("name=\"" + anchor + "\"") && !html.contains("id=\"" + anchor + "\""))
        {
            return testing::AssertionFailure() << "no anchor " << anchor << " in " << file;
        }
    }
    return testing::AssertionSuccess();
}

TEST(HelpBook, ControlFilesReferToExistingFiles)
{
    ASSERT_TRUE(std::filesystem::exists(helpDir() / "help.hhp")) << helpDir();
    const auto project = mandelbrotter::app::parseHelpProject(readFile(helpDir() / "help.hhp"));
    ASSERT_TRUE(project.has_value()) << project.error();
    EXPECT_FALSE(project->title.empty());
    for (const std::string& file :
         {project->defaultTopic, project->contentsFile, project->indexFile})
    {
        EXPECT_TRUE(std::filesystem::exists(helpDir() / file)) << file;
    }
}

TEST(HelpBook, ContentsAndIndexEntriesResolve)
{
    for (const std::string_view file : {"contents.hhc", "index.hhk"})
    {
        const std::string sitemap = readFile(helpDir() / file);
        const auto        entries = sitemapEntries(sitemap);
        EXPECT_EQ(entries.size(), attributeValues(sitemap, "value").size() / 2) << file;
        ASSERT_FALSE(entries.empty()) << file;
        for (const auto& [name, local] : entries)
        {
            EXPECT_FALSE(name.empty()) << file << ": " << local;
            EXPECT_TRUE(pageExists(local)) << file << ": " << name;
        }
    }
}

TEST(HelpBook, EveryPageIsInTheContents)
{
    const std::string     contents = readFile(helpDir() / "contents.hhc");
    std::set<std::string> listed;
    for (const auto& [name, local] : sitemapEntries(contents))
    {
        listed.insert(local.substr(0, local.find('#')));
    }
    for (const std::filesystem::path& page : pages())
    {
        EXPECT_TRUE(listed.contains(page.filename().string())) << page.filename();
    }
}

TEST(HelpBook, PagesAreWellFormed)
{
    const auto allPages = pages();
    ASSERT_FALSE(allPages.empty());
    for (const std::filesystem::path& page : allPages)
    {
        SCOPED_TRACE(page.filename().string());
        const std::string html = readFile(page);
        EXPECT_NE(html.find("<title>"), std::string::npos);
        EXPECT_EQ(html.find("<style"), std::string::npos);
        EXPECT_EQ(html.find("<script"), std::string::npos);
        EXPECT_TRUE(std::ranges::all_of(html, [](char c) {
            return static_cast<unsigned char>(c) < 0x80;
        })) << "non-ASCII byte";
        if (page.filename() != "index.html")
        {
            EXPECT_NE(html.find("href=\"index.html\""), std::string::npos) << "no Contents link";
        }
    }
}

TEST(HelpBook, LinksAndImagesResolve)
{
    const mandelbrotter::RenderSettings defaults;
    for (const std::filesystem::path& page : pages())
    {
        SCOPED_TRACE(page.filename().string());
        const std::string html = readFile(page);
        for (const std::string& href : attributeValues(html, "href"))
        {
            SCOPED_TRACE(href);
            if (href.starts_with("http://") || href.starts_with("https://"))
            {
                continue;
            }
            if (mandelbrotter::isHelpActionUrl(href))
            {
                const auto action = mandelbrotter::parseHelpAction(href);
                ASSERT_TRUE(action.has_value()) << action.error();
                if (const auto* view = std::get_if<mandelbrotter::ViewAction>(&*action))
                {
                    const auto resolved = mandelbrotter::resolveViewAction(defaults, *view);
                    EXPECT_TRUE(resolved.has_value()) << resolved.error();
                }
                else if (const auto* flight = std::get_if<mandelbrotter::FlightAction>(&*action))
                {
                    EXPECT_NE(mandelbrotter::findFlight(flight->id), nullptr);
                }
                continue;
            }
            EXPECT_TRUE(pageExists(href));
        }
        for (const std::string& src : attributeValues(html, "src"))
        {
            EXPECT_TRUE(std::filesystem::exists(helpDir() / src)) << src;
        }
    }
}

TEST(HelpBook, ImagesAreUsedAndRenderedOnesHaveSpecs)
{
    std::set<std::string> referenced;
    for (const std::filesystem::path& page : pages())
    {
        for (const std::string& src : attributeValues(readFile(page), "src"))
        {
            referenced.insert(src);
        }
    }
    std::set<std::string> rendered;
    for (const mandelbrotter::HelpImageSpec& spec : mandelbrotter::helpImageSpecs())
    {
        rendered.insert(spec.file);
        EXPECT_TRUE(std::filesystem::exists(helpDir() / "images" / spec.file))
            << spec.file << " (run Mandelbrotter --screenshots docs/help/images)";
    }
    auto files = imageFiles();
    EXPECT_FALSE(files.empty()) << "no images (run Mandelbrotter --screenshots docs/help/images)";
    // The Qt build's screenshots stand in for the wx ones under the same images/ name.
    std::ranges::copy(imageFiles("images-qt"), std::back_inserter(files));
    for (const std::filesystem::path& file : files)
    {
        const std::string name = file.filename().string();
        SCOPED_TRACE(name);
        EXPECT_TRUE(file.extension() == ".png");
        EXPECT_TRUE(referenced.contains("images/" + name)) << "no page shows it";
        if (!name.starts_with("ui-"))
        {
            EXPECT_TRUE(rendered.contains(name)) << "neither a screenshot nor a rendered example";
        }
    }
}

TEST(HelpBook, EveryFlightIsDocumented)
{
    const std::string demos = readFile(helpDir() / "demos.html");
    for (const mandelbrotter::Flight& flight : mandelbrotter::builtinFlights())
    {
        EXPECT_NE(demos.find("mandelbrotter:flight " + flight.id), std::string::npos) << flight.id;
    }
}

TEST(HelpBook, EachBuildsImagesStayWithinBudget)
{
    // Each executable embeds the rendered examples and one set of screenshots: its toolkit's.
    std::uintmax_t rendered = 0;
    std::uintmax_t wxShots  = 0;
    for (const std::filesystem::path& file : imageFiles())
    {
        (isScreenshot(file) ? wxShots : rendered) += std::filesystem::file_size(file);
    }
    std::uintmax_t qtShots = 0;
    for (const std::filesystem::path& file : imageFiles())
    {
        const std::filesystem::path qtFile = helpDir() / "images-qt" / file.filename();
        if (isScreenshot(file))
        {
            qtShots += std::filesystem::file_size(std::filesystem::exists(qtFile) ? qtFile : file);
        }
    }
    EXPECT_LE(rendered + wxShots, kImageBudgetBytes) << "the wx build's book is getting large";
    EXPECT_LE(rendered + qtShots, kImageBudgetBytes) << "the Qt build's book is getting large";
}

TEST(HelpBook, QtScreenshotsMirrorTheWxSet)
{
    const auto qtFiles = imageFiles("images-qt");
    if (qtFiles.empty())
    {
        GTEST_SKIP() << "no Qt screenshots (the Qt build shows the wx ones)";
    }
    std::set<std::string> wx;
    for (const std::filesystem::path& file : imageFiles())
    {
        if (isScreenshot(file))
        {
            wx.insert(file.filename().string());
        }
    }
    std::set<std::string> qt;
    for (const std::filesystem::path& file : qtFiles)
    {
        EXPECT_TRUE(isScreenshot(file) && file.extension() == ".png") << file.filename();
        qt.insert(file.filename().string());
    }
    EXPECT_EQ(qt, wx) << "run the Qt build's --screenshots docs/help/images-qt";
}

}  // namespace
