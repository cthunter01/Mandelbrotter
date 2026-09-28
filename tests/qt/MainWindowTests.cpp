#include "qt/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QSpinBox>
#include <QStatusTipEvent>
#include <QString>
#include <QTest>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/commands.h"
#include "Mandelbrotter/app/help_routing.h"
#include "Mandelbrotter/app/scenes.h"
#include "Mandelbrotter/flights.h"
#include "Mandelbrotter/help_action.h"
#include "QtHarness.h"
#include "TempDir.h"
#include "qt/ElidedLabel.h"
#include "qt/FractalCanvas.h"
#include "qt/SidePanel.h"
#include "qt/qt_util.h"

namespace
{

using namespace Qt::StringLiterals;
using mandelbrotter::app::Command;
using mandelbrotter::app::MenuEntry;
using mandelbrotter::app::StatusField;
using mandelbrotter::test::pumpUntil;
using mandelbrotter::test::QtHarness;
namespace app = mandelbrotter::app;
namespace qt  = mandelbrotter::qt;

/// Every item of the menu table, submenus included.
void collectItems(const std::vector<MenuEntry>& entries, std::vector<MenuEntry>& into)
{
    for (const MenuEntry& entry : entries)
    {
        if (entry.kind == MenuEntry::Kind::SUBMENU)
        {
            collectItems(entry.children, into);
        }
        else if (entry.kind != MenuEntry::Kind::SEPARATOR)
        {
            into.push_back(entry);
        }
    }
}

std::vector<MenuEntry> allItems()
{
    std::vector<MenuEntry> items;
    for (const app::Menu& menu : app::menuBar())
    {
        collectItems(menu.entries, items);
    }
    return items;
}

QMenu* topMenu(const QtHarness& h, const QString& title)
{
    for (const QAction* action : h.window.menuBar()->actions())
    {
        if (action->text() == title)
        {
            return action->menu();
        }
    }
    return nullptr;
}

TEST(MainWindow, EveryMenuEntryIsAnAction)
{
    const QtHarness h;
    for (const MenuEntry& entry : allItems())
    {
        SCOPED_TRACE(entry.label);
        const QAction* action = h.window.action(entry.command, entry.flight);
        ASSERT_NE(action, nullptr);
        EXPECT_EQ(action->text(), qt::toQt(entry.label));
        EXPECT_EQ(action->shortcut(),
                  QKeySequence::fromString(qt::toQt(entry.shortcut), QKeySequence::PortableText));
        EXPECT_EQ(action->statusTip(), qt::toQt(entry.statusTip));
        EXPECT_EQ(action->isCheckable(), entry.kind == MenuEntry::Kind::CHECK);
        if (entry.kind == MenuEntry::Kind::CHECK)
        {
            EXPECT_EQ(action->isChecked(), entry.checked);
        }
        switch (entry.role)
        {
            case app::MenuRole::QUIT:
                EXPECT_EQ(action->menuRole(), QAction::QuitRole);
                break;
            case app::MenuRole::ABOUT:
                EXPECT_EQ(action->menuRole(), QAction::AboutRole);
                break;
            case app::MenuRole::CONTENTS:
            case app::MenuRole::NONE:
                EXPECT_EQ(action->menuRole(), QAction::NoRole);
                break;
        }
    }
    ASSERT_NE(h.window.aboutQtAction(), nullptr);
    EXPECT_EQ(h.window.aboutQtAction()->menuRole(), QAction::AboutQtRole);

    std::vector<QString> titles;
    for (const QAction* menu : h.window.menuBar()->actions())
    {
        titles.push_back(menu->text());
    }
    EXPECT_EQ(titles, (std::vector<QString>{u"&File"_s, u"&View"_s, u"&Bookmarks"_s, u"&Help"_s}));
    EXPECT_EQ(h.window.windowTitle(), u"Mandelbrotter"_s);
}

TEST(MainWindow, TheHelpMenuUpdatesItsDemoItemsWhenItOpens)
{
    QtHarness h;
    QAction*  stop = h.window.action(Command::STOP_DEMO);
    QAction*  back = h.window.action(Command::BACK_TO_SNAPSHOT);
    ASSERT_NE(stop, nullptr);
    ASSERT_NE(back, nullptr);
    EXPECT_FALSE(stop->isEnabled());
    EXPECT_FALSE(back->isEnabled());

    h.window.action(Command::FLIGHT, 0)->trigger();
    EXPECT_TRUE(h.window.app().flightPlaying());
    QMenu* help = topMenu(h, u"&Help"_s);
    ASSERT_NE(help, nullptr);
    Q_EMIT help->aboutToShow();
    EXPECT_TRUE(stop->isEnabled());
    EXPECT_TRUE(back->isEnabled());

    stop->trigger();
    EXPECT_FALSE(h.window.app().flightPlaying());
    Q_EMIT help->aboutToShow();
    EXPECT_FALSE(stop->isEnabled());
    back->trigger();
    Q_EMIT help->aboutToShow();
    EXPECT_FALSE(back->isEnabled());
}

TEST(MainWindow, TheZoomItemsChangeTheZoomField)
{
    QtHarness h;
    EXPECT_EQ(h.status(StatusField::ZOOM), "Zoom 1x");
    h.window.action(Command::ZOOM_IN)->trigger();
    EXPECT_EQ(h.status(StatusField::ZOOM), "Zoom 2x");
    h.window.action(Command::ZOOM_OUT)->trigger();
    h.window.action(Command::ZOOM_OUT)->trigger();
    EXPECT_EQ(h.status(StatusField::ZOOM), "Zoom 0.5x");
    h.window.action(Command::RESET_VIEW)->trigger();
    EXPECT_EQ(h.status(StatusField::ZOOM), "Zoom 1x");
}

TEST(MainWindow, AHoveredItemsTipCoversThePointerField)
{
    QtHarness h;
    h.window.app().canvas().showOrbitAt({0.25, 0.5});  // the pointer field shows a point
    const std::string pointer = h.status(StatusField::POINTER);
    ASSERT_FALSE(pointer.empty());
    const std::string center = h.status(StatusField::CENTER);

    QStatusTipEvent tip(u"Open the user guide"_s);
    QApplication::sendEvent(h.window.menuBar(), &tip);
    EXPECT_EQ(h.status(StatusField::POINTER), "Open the user guide");
    EXPECT_EQ(h.status(StatusField::CENTER), center);  // the other fields stay

    QStatusTipEvent none{QString()};
    QApplication::sendEvent(h.window.menuBar(), &none);
    EXPECT_EQ(h.status(StatusField::POINTER), pointer);
}

TEST(MainWindow, ALongStatusTextDoesNotWidenTheWindow)
{
    QtHarness        h;
    const int        before = h.window.minimumSizeHint().width();
    qt::ElidedLabel& center = h.window.statusField(StatusField::CENTER);
    center.setFullText(QString(400, u'7'));
    QApplication::processEvents();
    EXPECT_EQ(h.window.minimumSizeHint().width(), before);
    EXPECT_LT(center.text().size(), 400);  // elided
    EXPECT_EQ(center.toolTip(), QString(400, u'7'));
}

TEST(MainWindow, TheSidePanelItemFollowsTheDock)
{
    QtHarness h;
    QAction*  show = h.window.action(Command::SHOW_PANEL);
    ASSERT_NE(show, nullptr);
    ASSERT_TRUE(pumpUntil([&] { return h.window.panelDock().isVisible(); }));
    EXPECT_TRUE(show->isChecked());
    h.window.panelDock().close();  // its own close button
    EXPECT_FALSE(show->isChecked());
    show->trigger();
    EXPECT_TRUE(h.window.panelDock().isVisible());
    EXPECT_TRUE(show->isChecked());
    h.window.setSidePanelShown(false);
    EXPECT_FALSE(show->isChecked());
}

TEST(MainWindow, TheOrbitItemAndCheckboxFollowTheOverlay)
{
    QtHarness  h;
    QAction*   orbit = h.window.action(Command::SHOW_ORBIT);
    QCheckBox* box   = h.window.panel().controls().orbit;
    ASSERT_NE(orbit, nullptr);
    orbit->trigger();
    EXPECT_TRUE(h.window.app().showOrbit());
    EXPECT_TRUE(box->isChecked());
    box->click();
    EXPECT_FALSE(h.window.app().showOrbit());
    EXPECT_FALSE(orbit->isChecked());
    h.window.app().setShowOrbit(true);
    EXPECT_TRUE(orbit->isChecked());
    EXPECT_TRUE(box->isChecked());
    h.window.app().runHelpAction(mandelbrotter::OrbitAction{.on = false});
    EXPECT_FALSE(orbit->isChecked());
    EXPECT_FALSE(box->isChecked());
}

TEST(MainWindow, EscapeStopsAFlightAndTheTour)
{
    QtHarness h;
    h.activate();
    h.window.canvas().setFocus();
    h.window.app().startFlight("seahorse-dive");
    QTest::keyClick(&h.window.canvas(), Qt::Key_Escape);
    EXPECT_FALSE(h.window.app().flightPlaying());

    h.window.app().startTour();
    h.window.panel().controls().family->setFocus();
    QTest::keyClick(h.window.panel().controls().family, Qt::Key_Escape);
    EXPECT_FALSE(h.window.app().tourRunning());
}

TEST(MainWindow, AnyKeyEndsAFlightAndStillDoesItsJob)
{
    QtHarness h;
    h.activate();
    h.window.canvas().setFocus();
    h.window.app().startFlight("palette-sweep");
    QTest::keyClick(&h.window.canvas(), Qt::Key_Plus);
    EXPECT_FALSE(h.window.app().flightPlaying());
    EXPECT_DOUBLE_EQ(
        h.window.app().settings().view.zoom,
        mandelbrotter::builtinFlights().back().keyframes.front().settings.view.zoom * 2.0);

    // A menu shortcut never reaches the control as a key press, but ends a flight too.
    h.window.app().startFlight("palette-sweep");
    QTest::keyClick(&h.window.canvas(), Qt::Key_Home, Qt::ControlModifier);
    EXPECT_FALSE(h.window.app().flightPlaying());
    EXPECT_DOUBLE_EQ(h.window.app().settings().view.zoom, 1.0);
}

TEST(MainWindow, MenuShortcutsBeatTextFields)
{
    QtHarness    h(mandelbrotter::app::juliaExample());
    const double home = h.window.app().settings().view.zoom;
    h.activate();
    h.window.action(Command::ZOOM_IN)->trigger();
    ASSERT_DOUBLE_EQ(h.window.app().settings().view.zoom, home * 2.0);
    QLineEdit* seed = h.window.panel().controls().seedRe;
    seed->setFocus();
    // A line edit would take Ctrl+Home for itself (to the start of the text).
    QTest::keyClick(seed, Qt::Key_Home, Qt::ControlModifier);
    EXPECT_DOUBLE_EQ(h.window.app().settings().view.zoom, home);
    QTest::keyClick(seed, Qt::Key_Plus, Qt::ControlModifier);
    EXPECT_DOUBLE_EQ(h.window.app().settings().view.zoom, home * 2.0);
    EXPECT_TRUE(seed->hasFocus());
}

TEST(MainWindow, HelpForTheFocusedControl)
{
    QtHarness h;
    h.activate();
    const auto page = [&h] { return std::string(app::helpPageFor(h.window.helpContext())); };
    h.window.canvas().setFocus();
    EXPECT_EQ(page(), "navigating.html");
    h.window.panel().controls().seedRe->setFocus();
    EXPECT_EQ(page(), "julia.html");
    h.window.panel().controls().exponent->setFocus();
    EXPECT_EQ(page(), "fractals.html");
    h.window.panel().controls().iterations->setFocus();
    EXPECT_EQ(page(), "iterations.html");
    h.window.panel().controls().bookmarks->setFocus();
    EXPECT_EQ(page(), "bookmarks.html");
    QApplication::focusWidget()->clearFocus();
    EXPECT_EQ(page(), "index.html");
}

TEST(MainWindow, ClosingWithAFocusedSeedFieldAndAFlightIsClean)
{
    const mandelbrotter::test::TempDir dir;
    auto window = std::make_unique<qt::MainWindow>(app::juliaExample(), dir / "bookmarks.json");
    window->show();
    window->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(window.get()));
    QLineEdit* seed = window->panel().controls().seedRe;
    seed->setFocus();
    seed->setText(u"0.3"_s);  // edited, not committed: committing it is the danger
    window->app().startFlight("julia-sweep");
    window->close();
    window.reset();
    QApplication::processEvents();
}

}  // namespace
