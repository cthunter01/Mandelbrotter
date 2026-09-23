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

std::vector<std::filesystem::path> imageFiles()
{
    std::vector<std::filesystem::path> result;
    for (const auto& entry : std::filesystem::directory_iterator(helpDir() / "images"))
    {
        if (entry.is_regular_file())
        {
            result.push_back(entry.path());
        }
    }
    std::ranges::sort(result);
    return result;
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

/// The (Name, Local) pairs of a sitemap file (.hhc / .hhk).
std::vector<std::pair<std::string, std::string>> sitemapEntries(const std::string& sitemap)
{
    const std::regex pattern(
        R"re(<param name="Name" value="([^"]*)">\s*<param name="Local" value="([^"]*)">)re",
        std::regex::icase);
    std::vector<std::pair<std::string, std::string>> entries;
    for (std::sregex_iterator it(sitemap.begin(), sitemap.end(), pattern), end; it != end; ++it)
    {
        entries.emplace_back((*it)[1].str(), (*it)[2].str());
    }
    return entries;
}

std::string hhpValue(const std::string& hhp, std::string_view key)
{
    const std::string prefix = std::string(key) + "=";
    const auto        at     = hhp.find(prefix);
    if (at == std::string::npos)
    {
        return {};
    }
    const auto end = hhp.find('\n', at);
    return hhp.substr(at + prefix.size(),
                      end == std::string::npos ? end : end - at - prefix.size());
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
    const std::string hhp = readFile(helpDir() / "help.hhp");
    EXPECT_FALSE(hhpValue(hhp, "Title").empty());
    for (const std::string_view key : {"Default topic", "Contents file", "Index file"})
    {
        const std::string value = hhpValue(hhp, key);
        EXPECT_FALSE(value.empty()) << key;
        EXPECT_TRUE(std::filesystem::exists(helpDir() / value)) << key << " = " << value;
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
    const auto files = imageFiles();
    EXPECT_FALSE(files.empty()) << "no images (run Mandelbrotter --screenshots docs/help/images)";
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

TEST(HelpBook, ImagesStayWithinBudget)
{
    std::uintmax_t total = 0;
    for (const std::filesystem::path& file : imageFiles())
    {
        total += std::filesystem::file_size(file);
    }
    EXPECT_LE(total, kImageBudgetBytes) << "the embedded book is getting large";
}

}  // namespace
