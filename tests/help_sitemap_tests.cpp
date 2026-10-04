#include "Mandelbrotter/app/help_sitemap.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using mandelbrotter::app::HelpSearch;
using mandelbrotter::app::HelpTopic;
namespace app = mandelbrotter::app;

std::filesystem::path helpDir()
{
    return std::filesystem::path{MANDELBROTTER_HELP_DIR};
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::size_t countTopics(const std::vector<HelpTopic>& topics)
{
    std::size_t count = topics.size();
    for (const HelpTopic& topic : topics)
    {
        count += countTopics(topic.children);
    }
    return count;
}

std::vector<std::string> pagesOf(const std::vector<HelpSearch::Hit>& hits)
{
    std::vector<std::string> pages;
    pages.reserve(hits.size());
    for (const HelpSearch::Hit& hit : hits)
    {
        pages.push_back(hit.page);
    }
    return pages;
}

/// The book's pages in contents order, as a help viewer would hand them to HelpSearch.
std::vector<std::pair<std::string, std::string>> bookPages()
{
    std::vector<std::pair<std::string, std::string>> pages;
    for (const HelpTopic& topic : app::parseHelpSitemap(readFile(helpDir() / "contents.hhc")))
    {
        const std::string page = topic.local.substr(0, topic.local.find('#'));
        if (std::ranges::none_of(pages, [&](const auto& entry) { return entry.first == page; }))
        {
            pages.emplace_back(page, readFile(helpDir() / page));
        }
    }
    return pages;
}

TEST(HelpSitemap, NestedListsBelongToTheEntryBeforeThem)
{
    const auto                   topics = app::parseHelpSitemap(R"(<HTML><BODY>
<UL>
  <LI><OBJECT type="text/sitemap"><param name="Name" value="One"><param name="Local" value="one.html"></OBJECT>
  <UL>
    <LI><OBJECT type="text/sitemap"><param name="Name" value="One &amp; a half"><param name="Local" value="one.html#half"></OBJECT>
    <UL>
      <li><object TYPE="text/sitemap"><PARAM NAME="Local" VALUE='deep.html'><param name="Name" value="Deep"></object>
    </UL>
  </UL>
  <LI><OBJECT type="text/sitemap"><param name="Name" value="Two"><param name="Local" value="two.html"></OBJECT>
  <!-- <LI><OBJECT type="text/sitemap"><param name="Name" value="Commented out"></OBJECT> -->
  <LI><OBJECT type="text/html"><param name="Name" value="Not a topic"></OBJECT>
</UL>
</BODY></HTML>)");
    const std::vector<HelpTopic> expected{
        {
            .name  = "One",
            .local = "one.html",
            .children =
                {
                    {
                        .name     = "One & a half",
                        .local    = "one.html#half",
                        .children = {{.name = "Deep", .local = "deep.html", .children = {}}},
                    },
                },
        },
        {.name = "Two", .local = "two.html", .children = {}},
    };
    EXPECT_EQ(topics, expected);
}

TEST(HelpSitemap, TheBooksContentsAreATreeAndItsIndexIsFlat)
{
    const auto contents = app::parseHelpSitemap(readFile(helpDir() / "contents.hhc"));
    EXPECT_EQ(countTopics(contents), 22U);
    ASSERT_FALSE(contents.empty());
    EXPECT_EQ(contents.front().name, "Welcome");
    EXPECT_EQ(contents.front().local, "index.html");
    const auto navigating =
        std::ranges::find(contents, std::string("navigating.html"), &HelpTopic::local);
    ASSERT_NE(navigating, contents.end());
    ASSERT_EQ(navigating->children.size(), 4U);
    EXPECT_EQ(navigating->children.front().local, "navigating.html#mouse");

    const auto index = app::parseHelpSitemap(readFile(helpDir() / "index.hhk"));
    EXPECT_EQ(index.size(), 54U);
    EXPECT_EQ(countTopics(index), 54U);
}

TEST(HelpSitemap, TheProjectFile)
{
    const auto project = app::parseHelpProject(readFile(helpDir() / "help.hhp"));
    ASSERT_TRUE(project.has_value()) << project.error();
    EXPECT_EQ(project->title, "Mandelbrotter Help");
    EXPECT_EQ(project->defaultTopic, "index.html");
    EXPECT_EQ(project->contentsFile, "contents.hhc");
    EXPECT_EQ(project->indexFile, "index.hhk");

    const auto crlf = app::parseHelpProject(
        "[OPTIONS]\r\nTitle=T\r\nDefault topic=a.html\r\nContents file=c.hhc\r\nIndex "
        "file=i.hhk\r\n");
    ASSERT_TRUE(crlf.has_value());
    EXPECT_EQ(crlf->indexFile, "i.hhk");

    const auto missing = app::parseHelpProject("[OPTIONS]\nTitle=T\nDefault topic=a.html\n");
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error(), "the help project has no \"Contents file\"");
}

TEST(HelpSitemap, TitlesAndTextDecodeEntitiesAndDropTags)
{
    const std::string html =
        "<html><head><TITLE> Keys &amp; the\n mouse </TITLE></head><body><h1>A&lt;B&gt;C</h1>"
        "<p>Say &quot;hi&quot;&nbsp;there<br>again &#65;&#x42; &unknown;</p><!-- hidden --></body>"
        "</html>";
    EXPECT_EQ(app::pageTitle(html), "Keys & the mouse");
    EXPECT_EQ(app::pageText(html), "Keys & the mouse A<B>C Say \"hi\" there again AB &unknown;");
    EXPECT_EQ(app::pageTitle("<p>no title</p>"), "");
}

TEST(HelpSearch, EveryWordMustOccurAndMoreHitsRankFirst)
{
    const std::vector<std::pair<std::string, std::string>> pages{
        {"a.html", "<title>A</title><p>orbit orbit zoom</p>"},
        {"b.html", "<title>B</title><p>Orbit zoom</p>"},
        {"c.html", "<title>C</title><p>orbits and more orbits</p>"},
        {"d.html", "<title>D</title><p>orbit zoom</p>"},
    };
    const HelpSearch search(pages);

    const auto hits = search.find("orbit", false, false);
    EXPECT_EQ(pagesOf(hits), (std::vector<std::string>{"a.html", "c.html", "b.html", "d.html"}));
    EXPECT_EQ(hits.front(), (HelpSearch::Hit{.page = "a.html", .title = "A", .count = 2}));

    // Whole words: "orbits" no longer counts.
    EXPECT_EQ(pagesOf(search.find("orbit", false, true)),
              (std::vector<std::string>{"a.html", "b.html", "d.html"}));
    // Case: only b.html writes "Orbit".
    EXPECT_EQ(pagesOf(search.find("Orbit", true, false)), std::vector<std::string>{"b.html"});
    // Several words must all occur; ties keep the contents order.
    EXPECT_EQ(pagesOf(search.find("  zoom   orbit ", false, true)),
              (std::vector<std::string>{"a.html", "b.html", "d.html"}));
    EXPECT_EQ(pagesOf(search.find("zoom more", false, false)), std::vector<std::string>{});
    EXPECT_TRUE(search.find("   ", false, false).empty());
    // Markup is not text.
    EXPECT_TRUE(search.find("title", false, false).empty());
}

TEST(HelpSearch, FindsSeahorseInTheBook)
{
    const auto       pages = bookPages();
    const HelpSearch search(pages);
    const auto       any = pagesOf(search.find("seahorse", false, false));
    EXPECT_TRUE(std::ranges::find(any, "navigating.html") != any.end());
    EXPECT_TRUE(std::ranges::find(any, "demos.html") != any.end());

    // The capitalized name is more common in the text than the lower-case word.
    const auto upper = search.find("Seahorse", true, false);
    const auto lower = search.find("seahorse", true, false);
    EXPECT_FALSE(upper.empty());
    EXPECT_LT(lower.size(), any.size());
    EXPECT_LE(upper.size(), any.size());
    for (const HelpSearch::Hit& hit : search.find("seahorse", false, false))
    {
        EXPECT_FALSE(hit.title.empty()) << hit.page;
    }
}

}  // namespace
