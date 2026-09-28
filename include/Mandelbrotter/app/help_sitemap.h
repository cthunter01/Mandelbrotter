#pragma once

#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// The help book's files read without a toolkit: the project file, the contents and index sitemaps,
/// page titles and text, and a full-text search. A help viewer that is not wx's own (which reads
/// the book itself) builds its contents tree, index and search from these.
namespace mandelbrotter::app
{

/// The [OPTIONS] of a help project file (help.hhp).
struct HelpProject
{
    std::string title;         ///< "Mandelbrotter Help"
    std::string defaultTopic;  ///< "index.html"
    std::string contentsFile;  ///< "contents.hhc"
    std::string indexFile;     ///< "index.hhk"
};
/// Fails when one of the four is missing.
[[nodiscard]] std::expected<HelpProject, std::string> parseHelpProject(std::string_view hhp);

/// One entry of a contents tree or index.
struct HelpTopic
{
    std::string            name;
    std::string            local;  ///< the page, optionally with an anchor: "navigating.html#mouse"
    std::vector<HelpTopic> children;  ///< the entries of a <UL> that follows this one

    bool operator==(const HelpTopic&) const = default;
};
/// The <OBJECT type="text/sitemap"> entries of a .hhc (nested) or .hhk (flat) file, in order.
[[nodiscard]] std::vector<HelpTopic> parseHelpSitemap(std::string_view sitemap);

/// A page's <title>, entities decoded and spaces folded; empty when it has none.
[[nodiscard]] std::string pageTitle(std::string_view html);
/// What a page shows: tags dropped, entities decoded, runs of white space folded into one space.
[[nodiscard]] std::string pageText(std::string_view html);

/// Full-text search over the book's pages.
class HelpSearch
{
public:
    struct Hit
    {
        std::string page;     ///< the file name
        std::string title;    ///< its <title>
        int         count{};  ///< how often the query's words occur

        bool operator==(const Hit&) const = default;
    };

    /// `pages` in contents order: (file name, html).
    explicit HelpSearch(std::span<const std::pair<std::string, std::string>> pages);

    /// The pages holding every word of `query`, most occurrences first, contents order on ties.
    /// Case-insensitive unless `caseSensitive`; with `wholeWords` a word only matches where it is
    /// not part of a longer word ("orbit" does not match "orbits").
    [[nodiscard]] std::vector<Hit> find(std::string_view query, bool caseSensitive,
                                        bool wholeWords) const;

private:
    struct Page
    {
        std::string file;
        std::string title;
        std::string text;
        std::string lowerText;
    };
    std::vector<Page> m_pages;
};

}  // namespace mandelbrotter::app
