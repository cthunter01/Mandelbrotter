#include "qt/HelpWindow.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QString>
#include <QTest>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUrl>
#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/commands.h"
#include "Mandelbrotter/app/help_routing.h"
#include "Mandelbrotter/app/ui_text.h"
#include "Mandelbrotter/help_action.h"
#include "QtHarness.h"
#include "qt/MainWindow.h"
#include "qt/SidePanel.h"
#include "qt/qt_util.h"

namespace
{

using namespace Qt::StringLiterals;
using mandelbrotter::app::Command;
using mandelbrotter::test::pumpUntil;
using mandelbrotter::test::QtHarness;
namespace app = mandelbrotter::app;
namespace qt  = mandelbrotter::qt;

std::filesystem::path helpDir()
{
    return std::filesystem::path{MANDELBROTTER_HELP_DIR};
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<std::string> pages()
{
    std::vector<std::string> result;
    for (const auto& entry : std::filesystem::directory_iterator(helpDir()))
    {
        if (entry.path().extension() == ".html")
        {
            result.push_back(entry.path().filename().string());
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

/// A main window whose help window is open on the contents page.
struct HelpHarness : QtHarness
{
    HelpHarness()
    {
        window.dialogSeams.showError = [this](std::string_view title, std::string_view message) {
            errors.emplace_back(std::string(title) + ": " + std::string(message));
        };
        help().openExternal = [this](const QUrl& url) { opened.push_back(url); };
        window.action(Command::CONTENTS)->trigger();
    }

    [[nodiscard]] qt::HelpWindow& help() { return window.help(); }
    /// Clicks a link of the page shown.
    void click(const QString& href) { Q_EMIT help().browser().anchorClicked(QUrl(href)); }

    std::vector<std::string> errors;
    std::vector<QUrl>        opened;
};

TEST(HelpWindow, EveryPageLoadsWithItsTitleAndPictures)
{
    HelpHarness h;
    for (const std::string& page : pages())
    {
        SCOPED_TRACE(page);
        h.help().showPage(page);
        EXPECT_EQ(h.help().currentPage(), page);
        EXPECT_FALSE(h.help().browser().documentTitle().isEmpty());
        EXPECT_EQ(h.help().windowTitle(),
                  qt::toQt(app::kHelpTitlePrefix) + h.help().browser().documentTitle());
        for (const std::string& src : attributeValues(readFile(helpDir() / page), "src"))
        {
            EXPECT_TRUE(QFile::exists(u":/help/"_s + qt::toQt(src))) << src;
        }
    }
    EXPECT_TRUE(h.errors.empty());
}

TEST(HelpWindow, ActionLinksSurviveQUrl)
{
    for (const std::string& page : pages())
    {
        for (const std::string& href : attributeValues(readFile(helpDir() / page), "href"))
        {
            if (!mandelbrotter::isHelpActionUrl(href))
            {
                continue;
            }
            SCOPED_TRACE(std::format("{}: {}", page, href));
            const std::string rebuilt = qt::helpActionText(QUrl(qt::toQt(href)));
            EXPECT_EQ(rebuilt, href);
            EXPECT_TRUE(mandelbrotter::parseHelpAction(rebuilt).has_value());
        }
    }
}

TEST(HelpWindow, ATryItLinkActsOnTheMainWindowAndRaisesIt)
{
    HelpHarness h;
    h.help().showPage("julia.html");
    ASSERT_TRUE(mandelbrotter::test::activate(h.help()));
    h.click(u"mandelbrotter:view --julia -0.8,0.156 --palette fire"_s);
    EXPECT_TRUE(h.window.app().settings().fractal.julia);
    EXPECT_EQ(h.window.app().settings().coloring.palette, "fire");
    EXPECT_TRUE(pumpUntil([&h] { return h.window.isActiveWindow(); }));
    EXPECT_EQ(h.help().currentPage(), "julia.html");  // the help stays where it was
    EXPECT_TRUE(h.window.app().hasSnapshot());
}

TEST(HelpWindow, AMalformedActionIsReported)
{
    HelpHarness h;
    h.click(u"mandelbrotter:fly-me-to-the-moon"_s);
    ASSERT_EQ(h.errors.size(), 1U);
    EXPECT_TRUE(h.errors[0].starts_with("Help: ")) << h.errors[0];
}

TEST(HelpWindow, ExternalLinksGoToTheBrowser)
{
    HelpHarness h;
    h.click(u"https://www.wxwidgets.org/"_s);
    ASSERT_EQ(h.opened.size(), 1U);
    EXPECT_EQ(h.opened[0], QUrl(u"https://www.wxwidgets.org/"_s));
    EXPECT_EQ(h.help().currentPage(), "index.html");
    h.click(u"bookmarks.html"_s);  // a page of the book
    EXPECT_EQ(h.help().currentPage(), "bookmarks.html");
}

TEST(HelpWindow, TheContentsTreeAndTheIndex)
{
    HelpHarness h;
    int         entries = 0;
    for (QTreeWidgetItemIterator it(&h.help().contentsTree()); *it != nullptr; ++it)
    {
        ++entries;
        if ((*it)->childCount() > 0)
        {
            EXPECT_TRUE((*it)->isExpanded());
        }
    }
    EXPECT_EQ(entries, 22);

    QListWidget& index   = h.help().indexList();
    const auto   visible = [&index] {
        int count = 0;
        for (int i = 0; i < index.count(); ++i)
        {
            count += index.item(i)->isHidden() ? 0 : 1;
        }
        return count;
    };
    EXPECT_EQ(visible(), 54);
    h.help().indexFilter().setText(u"BOOKMARK"_s);
    EXPECT_GT(visible(), 0);
    EXPECT_LT(visible(), 54);
    h.help().showAllButton().click();
    EXPECT_EQ(visible(), 54);
    EXPECT_TRUE(h.help().indexFilter().text().isEmpty());
}

TEST(HelpWindow, SearchFindsPages)
{
    HelpHarness h;
    h.help().searchField().setText(u"Seahorse"_s);
    h.help().search();
    const int any = h.help().searchResults().count();
    EXPECT_GT(any, 1);
    h.help().caseSensitiveBox().setChecked(true);
    h.help().wholeWordsBox().setChecked(true);
    h.help().search();
    EXPECT_GT(h.help().searchResults().count(), 0);
    EXPECT_LE(h.help().searchResults().count(), any);
    h.help().searchResults().item(0)->setSelected(true);
    Q_EMIT h.help().searchResults().itemActivated(h.help().searchResults().item(0));
    EXPECT_NE(h.help().currentPage(), "index.html");
}

TEST(HelpWindow, TheToolbarNavigates)
{
    HelpHarness h;
    EXPECT_FALSE(h.help().previousAction().isEnabled());  // the first page
    EXPECT_FALSE(h.help().upAction().isEnabled());
    h.help().nextAction().trigger();
    EXPECT_EQ(h.help().currentPage(), "getting-started.html");
    h.help().nextAction().trigger();
    h.help().nextAction().trigger();
    EXPECT_EQ(h.help().currentPage(), "navigating.html#mouse");
    ASSERT_TRUE(h.help().upAction().isEnabled());
    h.help().upAction().trigger();
    EXPECT_EQ(h.help().currentPage(), "navigating.html");
    h.help().previousAction().trigger();
    EXPECT_EQ(h.help().currentPage(), "getting-started.html");

    ASSERT_TRUE(h.help().backAction().isEnabled());
    h.help().backAction().trigger();
    EXPECT_EQ(h.help().currentPage(), "navigating.html");
    ASSERT_TRUE(h.help().forwardAction().isEnabled());
    h.help().forwardAction().trigger();
    EXPECT_EQ(h.help().currentPage(), "getting-started.html");
}

TEST(HelpWindow, AnUnknownPageShowsTheContents)
{
    HelpHarness h;
    h.help().showPage("orbit.html");
    h.help().showPage("no-such-page.html");
    EXPECT_EQ(h.help().currentPage(), "index.html");
}

TEST(HelpWindow, TheMenusAndF1OpenTheirPages)
{
    HelpHarness h;
    h.window.action(Command::REFERENCE)->trigger();
    EXPECT_EQ(h.help().currentPage(), std::string(app::kReferencePage));
    h.activate();
    h.window.panel().controls().palette->setFocus();
    h.window.action(Command::CONTEXT_HELP)->trigger();
    EXPECT_EQ(h.help().currentPage(), "coloring.html");
    EXPECT_TRUE(h.window.helpShown());
    h.window.close();  // the help window goes with the main window
    EXPECT_FALSE(h.window.helpShown());
}

}  // namespace
