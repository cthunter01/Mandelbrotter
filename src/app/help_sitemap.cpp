#include "Mandelbrotter/app/help_sitemap.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mandelbrotter::app
{

namespace
{

char lower(char c) noexcept
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string lowered(std::string_view text)
{
    std::string result(text);
    std::ranges::transform(result, result.begin(), lower);
    return result;
}

bool isSpace(char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

bool isWordChar(char c) noexcept
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

std::string_view trimmed(std::string_view text) noexcept
{
    while (!text.empty() && isSpace(text.front()))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && isSpace(text.back()))
    {
        text.remove_suffix(1);
    }
    return text;
}

/// Appends the UTF-8 encoding of `code` (a character reference).
void appendUtf8(std::string& out, unsigned code)
{
    if (code < 0x80)
    {
        out += static_cast<char>(code);
    }
    else if (code < 0x800)
    {
        out += static_cast<char>(0xC0 | (code >> 6));
        out += static_cast<char>(0x80 | (code & 0x3F));
    }
    else if (code < 0x10000)
    {
        out += static_cast<char>(0xE0 | (code >> 12));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    }
    else
    {
        out += static_cast<char>(0xF0 | (code >> 18));
        out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    }
}

/// The code point of a numeric character reference without its '#': "60" or "x3C".
std::optional<unsigned> characterCode(std::string_view digits)
{
    unsigned base = 10;
    if (!digits.empty() && (digits.front() == 'x' || digits.front() == 'X'))
    {
        base = 16;
        digits.remove_prefix(1);
    }
    if (digits.empty() || digits.size() > 8)
    {
        return std::nullopt;
    }
    unsigned code = 0;
    for (const char c : digits)
    {
        unsigned digit = 0;
        if (c >= '0' && c <= '9')
        {
            digit = static_cast<unsigned>(c - '0');
        }
        else if (base == 16 && lower(c) >= 'a' && lower(c) <= 'f')
        {
            digit = static_cast<unsigned>(lower(c) - 'a' + 10);
        }
        else
        {
            return std::nullopt;
        }
        code = code * base + digit;
    }
    if (code == 0 || code > 0x10FFFF)
    {
        return std::nullopt;
    }
    return code;
}

/// The character an entity stands for ("amp" -> "&", "#60" -> "<"); nullopt for one we do not
/// know, which then stays as written.
std::optional<std::string> entity(std::string_view name)
{
    struct Named
    {
        std::string_view name;
        std::string_view text;
    };
    static constexpr std::array kNamed{
        Named{"lt", "<"},          Named{"gt", ">"},           Named{"amp", "&"},
        Named{"quot", "\""},       Named{"apos", "'"},         Named{"nbsp", " "},
        Named{"copy", "\xC2\xA9"}, Named{"times", "\xC3\x97"}, Named{"minus", "\xE2\x88\x92"},
    };
    for (const Named& named : kNamed)
    {
        if (named.name == name)
        {
            return std::string(named.text);
        }
    }
    if (name.size() > 1 && name.front() == '#')
    {
        if (const auto code = characterCode(name.substr(1)))
        {
            std::string text;
            appendUtf8(text, *code);
            return text;
        }
    }
    return std::nullopt;
}

std::string decodeEntities(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size())
    {
        if (text[i] == '&')
        {
            const std::size_t semicolon = text.find(';', i + 1);
            if (semicolon != std::string_view::npos && semicolon - i <= 10)
            {
                if (const auto decoded = entity(text.substr(i + 1, semicolon - i - 1)))
                {
                    out += *decoded;
                    i = semicolon + 1;
                    continue;
                }
            }
        }
        out += text[i];
        ++i;
    }
    return out;
}

/// Runs of white space become one space; none at either end.
std::string foldSpaces(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    bool pendingSpace = false;
    for (const char c : text)
    {
        if (isSpace(c))
        {
            pendingSpace = !out.empty();
            continue;
        }
        if (pendingSpace)
        {
            out += ' ';
            pendingSpace = false;
        }
        out += c;
    }
    return out;
}

/// A tag: `<name attr="value" ...>` or `</name>`, name lowered.
struct Tag
{
    std::string      name;  ///< "ul", "/ul", "object", "param", ...
    std::string_view attributes;
};

/// The value of `attribute` (case-insensitive name) in a tag's attribute text, entities decoded.
std::optional<std::string> attributeValue(std::string_view attributes, std::string_view attribute)
{
    const std::string lower = lowered(attributes);
    std::size_t       from  = 0;
    while (true)
    {
        const std::size_t at = lower.find(attribute, from);
        if (at == std::string::npos)
        {
            return std::nullopt;
        }
        from = at + attribute.size();
        if (at > 0 && isWordChar(lower[at - 1]))
        {
            continue;  // the end of a longer name
        }
        std::size_t i = from;
        while (i < lower.size() && isSpace(lower[i]))
        {
            ++i;
        }
        if (i >= lower.size() || lower[i] != '=')
        {
            continue;
        }
        ++i;
        while (i < lower.size() && isSpace(lower[i]))
        {
            ++i;
        }
        if (i < lower.size() && (lower[i] == '"' || lower[i] == '\''))
        {
            const char        quote = lower[i];
            const std::size_t end   = lower.find(quote, i + 1);
            if (end == std::string::npos)
            {
                return std::nullopt;
            }
            return decodeEntities(attributes.substr(i + 1, end - i - 1));
        }
        std::size_t end = i;
        while (end < lower.size() && !isSpace(lower[end]) && lower[end] != '>')
        {
            ++end;
        }
        return decodeEntities(attributes.substr(i, end - i));
    }
}

/// Calls `onTag` for every tag of `html` in order (comments are skipped).
template <typename OnTag>
void forEachTag(std::string_view html, OnTag onTag)
{
    std::size_t i = 0;
    while ((i = html.find('<', i)) != std::string_view::npos)
    {
        if (html.substr(i).starts_with("<!--"))
        {
            const std::size_t end = html.find("-->", i + 4);
            i                     = end == std::string_view::npos ? html.size() : end + 3;
            continue;
        }
        const std::size_t end = html.find('>', i + 1);
        if (end == std::string_view::npos)
        {
            return;
        }
        const std::string_view inside  = html.substr(i + 1, end - i - 1);
        std::size_t            nameEnd = 0;
        while (nameEnd < inside.size() && !isSpace(inside[nameEnd]) && inside[nameEnd] != '/')
        {
            ++nameEnd;
        }
        if (nameEnd == 0 && !inside.empty() && inside.front() == '/')
        {
            nameEnd = 1;
            while (nameEnd < inside.size() && !isSpace(inside[nameEnd]))
            {
                ++nameEnd;
            }
        }
        onTag(
            Tag{.name = lowered(inside.substr(0, nameEnd)), .attributes = inside.substr(nameEnd)});
        i = end + 1;
    }
}

/// Builds the topic tree of a sitemap, one tag at a time.
class SitemapParser
{
public:
    void onTag(const Tag& tag)
    {
        if (tag.name == "ul")
        {
            openList();
        }
        else if (tag.name == "/ul")
        {
            if (!m_levels.empty())
            {
                m_levels.pop_back();
            }
        }
        else if (tag.name == "object")
        {
            const auto type = attributeValue(tag.attributes, "type");
            if (type && lowered(*type) == "text/sitemap")
            {
                m_current = HelpTopic{};
            }
        }
        else if (tag.name == "param")
        {
            readParam(tag);
        }
        else if (tag.name == "/object" && m_current)
        {
            (m_levels.empty() ? m_root : *m_levels.back()).push_back(std::move(*m_current));
            m_current.reset();
        }
    }

    [[nodiscard]] std::vector<HelpTopic> take() { return std::move(m_root); }

private:
    void openList()
    {
        if (m_levels.empty())
        {
            m_levels.push_back(&m_root);
            return;
        }
        // A nested list belongs to the entry before it.
        std::vector<HelpTopic>* parent = m_levels.back();
        m_levels.push_back(parent->empty() ? parent : &parent->back().children);
    }

    void readParam(const Tag& tag)
    {
        const auto name  = attributeValue(tag.attributes, "name");
        const auto value = attributeValue(tag.attributes, "value");
        if (!m_current || !name || !value)
        {
            return;
        }
        const std::string key = lowered(*name);
        if (key == "name")
        {
            m_current->name = *value;
        }
        else if (key == "local")
        {
            m_current->local = *value;
        }
    }

    std::vector<HelpTopic>               m_root;
    std::vector<std::vector<HelpTopic>*> m_levels;   ///< the list each open <UL> adds to
    std::optional<HelpTopic>             m_current;  ///< between <OBJECT> and </OBJECT>
};

/// Occurrences of `word` in `text`; with `wholeWords` only those not inside a longer word.
int countOccurrences(std::string_view text, std::string_view word, bool wholeWords)
{
    int         count = 0;
    std::size_t at    = 0;
    while ((at = text.find(word, at)) != std::string_view::npos)
    {
        const std::size_t end   = at + word.size();
        const bool        whole = (at == 0 || !isWordChar(text[at - 1])) &&
                                  (end == text.size() || !isWordChar(text[end]));
        if (!wholeWords || whole)
        {
            ++count;
        }
        at = end;
    }
    return count;
}

}  // namespace

std::expected<HelpProject, std::string> parseHelpProject(std::string_view hhp)
{
    HelpProject project;
    std::size_t start = 0;
    while (start <= hhp.size())
    {
        std::size_t end = hhp.find('\n', start);
        if (end == std::string_view::npos)
        {
            end = hhp.size();
        }
        const std::string_view line   = trimmed(hhp.substr(start, end - start));
        const std::size_t      equals = line.find('=');
        if (equals != std::string_view::npos)
        {
            const std::string_view key   = trimmed(line.substr(0, equals));
            const std::string      value = std::string(trimmed(line.substr(equals + 1)));
            if (key == "Title")
            {
                project.title = value;
            }
            else if (key == "Default topic")
            {
                project.defaultTopic = value;
            }
            else if (key == "Contents file")
            {
                project.contentsFile = value;
            }
            else if (key == "Index file")
            {
                project.indexFile = value;
            }
        }
        start = end + 1;
    }
    for (const auto& [value, key] :
         {std::pair{&project.title, "Title"}, std::pair{&project.defaultTopic, "Default topic"},
          std::pair{&project.contentsFile, "Contents file"},
          std::pair{&project.indexFile, "Index file"}})
    {
        if (value->empty())
        {
            return std::unexpected(std::string("the help project has no \"") + key + "\"");
        }
    }
    return project;
}

std::vector<HelpTopic> parseHelpSitemap(std::string_view sitemap)
{
    SitemapParser parser;
    forEachTag(sitemap, [&parser](const Tag& tag) { parser.onTag(tag); });
    return parser.take();
}

std::string pageTitle(std::string_view html)
{
    const std::string lower = lowered(html);
    const std::size_t open  = lower.find("<title>");
    if (open == std::string::npos)
    {
        return {};
    }
    const std::size_t begin = open + 7;
    const std::size_t close = lower.find("</title>", begin);
    if (close == std::string::npos)
    {
        return {};
    }
    return foldSpaces(decodeEntities(html.substr(begin, close - begin)));
}

std::string pageText(std::string_view html)
{
    std::string text;
    text.reserve(html.size());
    std::size_t i = 0;
    while (i < html.size())
    {
        if (html[i] != '<')
        {
            const std::size_t next = html.find('<', i);
            text.append(html.substr(i, next == std::string_view::npos ? next : next - i));
            i = next == std::string_view::npos ? html.size() : next;
            continue;
        }
        const bool        comment = html.substr(i).starts_with("<!--");
        const std::size_t end     = comment ? html.find("-->", i + 4) : html.find('>', i + 1);
        if (end == std::string_view::npos)
        {
            break;
        }
        text += ' ';  // a tag separates words ("a<br>b")
        i = end + (comment ? 3 : 1);
    }
    return foldSpaces(decodeEntities(text));
}

HelpSearch::HelpSearch(std::span<const std::pair<std::string, std::string>> pages)
{
    m_pages.reserve(pages.size());
    for (const auto& [file, html] : pages)
    {
        std::string text  = pageText(html);
        std::string lower = lowered(text);
        m_pages.push_back({.file      = file,
                           .title     = pageTitle(html),
                           .text      = std::move(text),
                           .lowerText = std::move(lower)});
    }
}

std::vector<HelpSearch::Hit> HelpSearch::find(std::string_view query, bool caseSensitive,
                                              bool wholeWords) const
{
    std::vector<std::string> words;
    const std::string        folded = foldSpaces(query);
    for (std::size_t start = 0; start < folded.size();)
    {
        std::size_t end = folded.find(' ', start);
        if (end == std::string::npos)
        {
            end = folded.size();
        }
        std::string word = folded.substr(start, end - start);
        words.push_back(caseSensitive ? std::move(word) : lowered(word));
        start = end + 1;
    }
    std::vector<Hit> hits;
    if (words.empty())
    {
        return hits;
    }
    for (const Page& page : m_pages)
    {
        const std::string& text  = caseSensitive ? page.text : page.lowerText;
        int                total = 0;
        bool               every = true;
        for (const std::string& word : words)
        {
            const int count = countOccurrences(text, word, wholeWords);
            if (count == 0)
            {
                every = false;
                break;
            }
            total += count;
        }
        if (every)
        {
            hits.push_back({.page = page.file, .title = page.title, .count = total});
        }
    }
    std::ranges::stable_sort(hits, [](const Hit& a, const Hit& b) { return a.count > b.count; });
    return hits;
}

}  // namespace mandelbrotter::app
