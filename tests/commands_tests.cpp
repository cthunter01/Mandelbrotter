#include "Mandelbrotter/app/commands.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <map>
#include <regex>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/flights.h"

namespace
{

using mandelbrotter::app::Command;
using mandelbrotter::app::EnableRule;
using mandelbrotter::app::Menu;
using mandelbrotter::app::MenuEntry;
using mandelbrotter::app::MenuRole;
using Kind    = MenuEntry::Kind;
namespace app = mandelbrotter::app;

/// Every item (not separators or submenus), submenus' items included, in menu order.
void collectItems(const std::vector<MenuEntry>& entries, std::vector<MenuEntry>& into)
{
    for (const MenuEntry& entry : entries)
    {
        if (entry.kind == Kind::SUBMENU)
        {
            collectItems(entry.children, into);
        }
        else if (entry.kind != Kind::SEPARATOR)
        {
            into.push_back(entry);
        }
    }
}

std::vector<MenuEntry> allItems()
{
    std::vector<MenuEntry> items;
    for (const Menu& menu : app::menuBar())
    {
        collectItems(menu.entries, items);
    }
    return items;
}

/// A label as the help book writes it: no mnemonic, no trailing "...".
std::string plain(std::string label)
{
    std::erase(label, '&');
    if (label.ends_with("..."))
    {
        label.resize(label.size() - 3);
    }
    return label;
}

struct ReferenceRow
{
    std::string shortcut;
    std::string menu;
    std::string item;
};

/// The rows of the Menus table in reference.html: "Ctrl+S" | "File &gt; Save image as PNG".
std::vector<ReferenceRow> referenceMenuRows()
{
    std::ifstream     in(std::filesystem::path{MANDELBROTTER_HELP_DIR} / "reference.html",
                         std::ios::binary);
    const std::string html{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    const auto        begin = html.find("<h2>Menus</h2>");
    const auto        end   = html.find("</table>", begin);
    if (begin == std::string::npos || end == std::string::npos)
    {
        return {};
    }
    const std::string table = html.substr(begin, end - begin);
    const std::regex  row(
        R"re(<tr><td>([^<]*)</td><td>([^<]*?) &gt; ([^<(]*?)( \([^<]*\))?</td></tr>)re");
    std::vector<ReferenceRow> rows;
    for (std::sregex_iterator it(table.begin(), table.end(), row), last; it != last; ++it)
    {
        rows.push_back(
            {.shortcut = (*it)[1].str(), .menu = (*it)[2].str(), .item = (*it)[3].str()});
    }
    return rows;
}

TEST(Commands, TheMenusAreFileViewBookmarksAndHelp)
{
    std::vector<std::string> titles;
    for (const Menu& menu : app::menuBar())
    {
        titles.push_back(menu.title);
    }
    EXPECT_EQ(titles, (std::vector<std::string>{"&File", "&View", "&Bookmarks", "&Help"}));
}

TEST(Commands, EveryCommandAppearsOnceAndEveryFlightHasItsItem)
{
    std::map<Command, int> count;
    for (const MenuEntry& entry : allItems())
    {
        ++count[entry.command];
    }
    for (int i = 0; i <= static_cast<int>(Command::ABOUT); ++i)
    {
        const auto command  = static_cast<Command>(i);
        const int  expected = command == Command::FLIGHT
                                  ? static_cast<int>(mandelbrotter::builtinFlights().size())
                                  : 1;
        EXPECT_EQ(count[command], expected) << "command " << i;
    }
}

TEST(Commands, LabelsAndShortcutsMatchTheReferencePage)
{
    const std::vector<ReferenceRow> rows = referenceMenuRows();
    ASSERT_EQ(rows.size(), 10U);
    const std::vector<Menu> menus = app::menuBar();
    for (const ReferenceRow& row : rows)
    {
        SCOPED_TRACE(row.menu + " > " + row.item);
        bool found = false;
        for (const Menu& menu : menus)
        {
            if (plain(menu.title) != row.menu)
            {
                continue;
            }
            std::vector<MenuEntry> items;
            collectItems(menu.entries, items);
            for (const MenuEntry& entry : items)
            {
                if (plain(entry.label) == row.item)
                {
                    found = true;
                    EXPECT_EQ(entry.shortcut, row.shortcut);
                }
            }
        }
        EXPECT_TRUE(found) << "no such menu item";
    }
    // ... and every shortcut is documented.
    int shortcuts = 0;
    for (const MenuEntry& entry : allItems())
    {
        shortcuts += entry.shortcut.empty() ? 0 : 1;
    }
    EXPECT_EQ(shortcuts, static_cast<int>(rows.size()));
}

TEST(Commands, TheDemosMenuListsTheFlightsThenStop)
{
    const std::vector<Menu> menus = app::menuBar();
    const Menu&             help  = menus.back();
    const MenuEntry*        demos = nullptr;
    for (const MenuEntry& entry : help.entries)
    {
        if (entry.kind == Kind::SUBMENU)
        {
            demos = &entry;
        }
    }
    ASSERT_NE(demos, nullptr);
    EXPECT_EQ(demos->label, "&Demos");
    EXPECT_EQ(demos->statusTip, "Animated dives into famous places");

    const auto flights = mandelbrotter::builtinFlights();
    ASSERT_EQ(demos->children.size(), flights.size() + 2);
    for (std::size_t i = 0; i < flights.size(); ++i)
    {
        const MenuEntry& entry = demos->children[i];
        EXPECT_EQ(entry.kind, Kind::ITEM);
        EXPECT_EQ(entry.command, Command::FLIGHT);
        EXPECT_EQ(entry.flight, i);
        EXPECT_EQ(entry.label, flights[i].title);
        EXPECT_EQ(entry.statusTip, flights[i].description);
    }
    EXPECT_EQ(demos->children[flights.size()].kind, Kind::SEPARATOR);
    const MenuEntry& stop = demos->children.back();
    EXPECT_EQ(stop.command, Command::STOP_DEMO);
    EXPECT_EQ(stop.enable, EnableRule::WHILE_DEMO);
}

TEST(Commands, RolesAndEnableRules)
{
    std::map<MenuRole, int> roles;
    for (const MenuEntry& entry : allItems())
    {
        ++roles[entry.role];
        switch (entry.command)
        {
            case Command::QUIT:
                EXPECT_EQ(entry.role, MenuRole::QUIT);
                break;
            case Command::ABOUT:
                EXPECT_EQ(entry.role, MenuRole::ABOUT);
                break;
            case Command::HELP_CONTENTS:
                EXPECT_EQ(entry.role, MenuRole::HELP_CONTENTS);
                break;
            case Command::STOP_DEMO:
                EXPECT_EQ(entry.enable, EnableRule::WHILE_DEMO);
                break;
            case Command::BACK_TO_SNAPSHOT:
                EXPECT_EQ(entry.enable, EnableRule::WITH_SNAPSHOT);
                break;
            default:
                EXPECT_EQ(entry.role, MenuRole::NONE);
                EXPECT_EQ(entry.enable, EnableRule::ALWAYS);
                break;
        }
    }
    EXPECT_EQ(roles[MenuRole::QUIT], 1);
    EXPECT_EQ(roles[MenuRole::ABOUT], 1);
    EXPECT_EQ(roles[MenuRole::HELP_CONTENTS], 1);
}

TEST(Commands, ThePanelStartsShownAndTheOrbitHidden)
{
    int checks = 0;
    for (const MenuEntry& entry : allItems())
    {
        if (entry.kind != Kind::CHECK)
        {
            continue;
        }
        ++checks;
        if (entry.command == Command::SHOW_PANEL)
        {
            EXPECT_TRUE(entry.checked);
        }
        else
        {
            EXPECT_EQ(entry.command, Command::SHOW_ORBIT);
            EXPECT_FALSE(entry.checked);
        }
    }
    EXPECT_EQ(checks, 2);
}

TEST(Commands, TextsAreAscii)
{
    for (const MenuEntry& entry : allItems())
    {
        for (const std::string& text : {entry.label, entry.shortcut, entry.statusTip})
        {
            for (const char c : text)
            {
                EXPECT_LT(static_cast<unsigned char>(c), 0x80) << text;
            }
        }
    }
}

}  // namespace
