#include "qt/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QEvent>
#include <QKeyCombination>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QStatusTipEvent>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/DemoPlayer.h"
#include "Mandelbrotter/app/commands.h"
#include "Mandelbrotter/app/help_routing.h"
#include "Mandelbrotter/app/ui_text.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/geometry.h"
#include "qt/ElidedLabel.h"
#include "qt/FractalCanvas.h"
#include "qt/GlobalKeyFilter.h"
#include "qt/SidePanel.h"
#include "qt/qt_util.h"

namespace mandelbrotter::qt
{

using namespace Qt::StringLiterals;

namespace
{

constexpr std::size_t index(app::StatusField field)
{
    return static_cast<std::size_t>(field);
}

QAction::MenuRole menuRole(app::MenuRole role)
{
    switch (role)
    {
        case app::MenuRole::QUIT:
            return QAction::QuitRole;
        case app::MenuRole::ABOUT:
            return QAction::AboutRole;
        case app::MenuRole::CONTENTS:
        case app::MenuRole::NONE:
            break;
    }
    // Never a role guessed from the text ("About", "Quit"), which would move items on macOS.
    return QAction::NoRole;
}

}  // namespace

MainWindow::MainWindow(RenderSettings initial, std::filesystem::path bookmarksPath)
  : m_canvas(new FractalCanvas(this, initial)),
    // Titled before its toggle action is taken: the action copies the title.
    m_dock(new QDockWidget(u"Settings"_s, this)),
    m_panel(new SidePanel(m_dock)),
    m_app(std::move(initial), std::move(bookmarksPath), m_canvas->controller(), makeShell())
{
    setWindowTitle(toQt(app::kWindowTitle));
    setCentralWidget(m_canvas);

    m_dock->setObjectName(u"SidePanelDock"_s);
    m_dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    m_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_dock->setWidget(m_panel);
    addDockWidget(Qt::RightDockWidgetArea, m_dock);

    buildMenus();
    buildStatusBar();
    resize(app::kWindowSize.width, app::kWindowSize.height + menuBar()->sizeHint().height() +
                                       statusBar()->sizeHint().height());

    wirePanel();
    connect(&m_demoTimer, &QTimer::timeout, this, [this] { m_app.tickDemo(); });
    m_keyFilter = new GlobalKeyFilter(
        *this,
        {.key            = [this](app::GlobalKey key) { return m_app.handleGlobalKey(key); },
         .isMenuShortcut = [this](const QKeyEvent& event) { return isMenuShortcut(event); }});
    QApplication::instance()->installEventFilter(m_keyFilter);

    m_app.start();
    refreshEnabledActions();
    m_canvas->setFocus();
}

MainWindow::~MainWindow()
{
    m_demoTimer.stop();
    QApplication::instance()->removeEventFilter(m_keyFilter);
    // The panel's controls outlive this window's members (a focused line edit reports
    // editingFinished while it is torn down); nothing they report may reach m_app now.
    m_panel->onSettingsChanged = nullptr;
    m_panel->onPickSeedToggled = nullptr;
    m_panel->onOrbitToggled    = nullptr;
    m_panel->onBookmarkAdd     = nullptr;
    m_panel->onBookmarkLoad    = nullptr;
    m_panel->onBookmarkDelete  = nullptr;
}

app::AppController::Shell MainWindow::makeShell()
{
    // Called before m_app exists; the hooks only run once it does.
    return {
        .setStatus     = [this](app::StatusField field,
                                std::string_view text) { setStatus(field, toQt(text)); },
        .reportError   = [this](std::string_view title,
                                std::string_view message) { reportError(title, message); },
        .panelSettings = [this](const RenderSettings& settings) { m_panel->setSettings(settings); },
        .panelEffectiveIterations =
            [this](int iterations) { m_panel->setEffectiveIterations(iterations); },
        .panelBookmarks = [this](std::span<const Bookmark> list) { m_panel->setBookmarks(list); },
        .panelPickSeedMode = [this](bool on) { m_panel->setPickSeedMode(on); },
        .showOrbitChanged =
            [this](bool on) {
                m_panel->setShowOrbit(on);
                if (QAction* orbit = action(app::Command::SHOW_ORBIT); orbit != nullptr)
                {
                    orbit->setChecked(on);
                }
            },
        .panelPreviewSeed  = [this](std::optional<Complex> seed) { m_panel->setPreviewSeed(seed); },
        .showExportDialog  = {},
        .closeExportDialog = {},
        .raiseWindow =
            [this] {
                if (isMinimized())
                {
                    showNormal();
                }
                raise();
                activateWindow();
            },
        .showHelpPage = {},
        .demoTimer =
            [this](bool on) {
                if (on)
                {
                    m_demoTimer.start(app::kDemoTick);
                }
                else
                {
                    m_demoTimer.stop();
                }
            },
        .now  = {},
        .tour = {},
    };
}

// ---------------------------------------------------------------------------------------------------------------
// Menus

void MainWindow::buildMenus()
{
    for (const app::Menu& menu : app::menuBar())
    {
        QMenu* items = menuBar()->addMenu(toQt(menu.title));
        addMenuEntries(*items, menu.entries);
    }
}

void MainWindow::addMenuEntries(QMenu& menu, const std::vector<app::MenuEntry>& entries)
{
    using Kind = app::MenuEntry::Kind;
    for (const app::MenuEntry& entry : entries)
    {
        switch (entry.kind)
        {
            case Kind::SEPARATOR:
                menu.addSeparator();
                break;
            case Kind::SUBMENU:
            {
                QMenu* submenu = menu.addMenu(toQt(entry.label));
                submenu->menuAction()->setStatusTip(toQt(entry.statusTip));
                addMenuEntries(*submenu, entry.children);
                break;
            }
            case Kind::ITEM:
            case Kind::CHECK:
                addCommandAction(menu, entry);
                break;
        }
    }
}

void MainWindow::addCommandAction(QMenu& menu, const app::MenuEntry& entry)
{
    QAction* item = nullptr;
    if (entry.command == app::Command::SHOW_PANEL)
    {
        // The dock's own action: it follows the dock however it is shown or hidden.
        item = m_dock->toggleViewAction();
        item->setText(toQt(entry.label));
        menu.addAction(item);
    }
    else
    {
        item = menu.addAction(toQt(entry.label));
        item->setCheckable(entry.kind == app::MenuEntry::Kind::CHECK);
        item->setChecked(entry.checked);
        // triggered, not toggled: showOrbitChanged sets the check from code.
        connect(item, &QAction::triggered, this,
                [this, command = entry.command, flight = entry.flight](bool checked) {
                    runCommand(command, flight, checked);
                });
    }
    if (!entry.shortcut.empty())
    {
        item->setShortcut(
            QKeySequence::fromString(toQt(entry.shortcut), QKeySequence::PortableText));
    }
    item->setStatusTip(toQt(entry.statusTip));
    item->setMenuRole(menuRole(entry.role));
    m_actions.push_back(
        {.command = entry.command, .flight = entry.flight, .enable = entry.enable, .action = item});
    if (entry.enable != app::EnableRule::ALWAYS)
    {
        connect(&menu, &QMenu::aboutToShow, this, [this] { refreshEnabledActions(); });
    }
    if (entry.command == app::Command::ABOUT)
    {
        // The customary courtesy for a program built on Qt (LGPL).
        m_aboutQt = menu.addAction(u"About &Qt"_s);
        m_aboutQt->setMenuRole(QAction::AboutQtRole);
        connect(m_aboutQt, &QAction::triggered, this, [] { QApplication::aboutQt(); });
    }
}

QAction* MainWindow::action(app::Command command, std::size_t flight) const
{
    for (const CommandAction& entry : m_actions)
    {
        if (entry.command == command && (command != app::Command::FLIGHT || entry.flight == flight))
        {
            return entry.action;
        }
    }
    return nullptr;
}

void MainWindow::refreshEnabledActions()
{
    for (const CommandAction& entry : m_actions)
    {
        if (entry.enable != app::EnableRule::ALWAYS)
        {
            entry.action->setEnabled(m_app.commandEnabled(entry.command));
        }
    }
}

void MainWindow::runCommand(app::Command command, std::size_t flight, bool checked)
{
    if (m_app.runCommand(command, flight))
    {
        return;
    }
    switch (command)
    {
        case app::Command::QUIT:
            close();
            break;
        case app::Command::SHOW_PANEL:
            setSidePanelShown(checked);
            break;
        case app::Command::SHOW_ORBIT:
            m_app.setShowOrbit(checked);
            break;
        case app::Command::ABOUT:
            showAbout();
            break;
        case app::Command::CONTEXT_HELP:
            showContextHelp();
            break;
        case app::Command::CONTENTS:
            m_app.showHelpPage(app::kContentsPage);
            break;
        case app::Command::REFERENCE:
            m_app.showHelpPage(app::kReferencePage);
            break;
        case app::Command::SAVE_IMAGE:
        case app::Command::COPY_IMAGE:
        case app::Command::EXPORT_VIEW:
        case app::Command::IMPORT_VIEW:
        case app::Command::ADD_BOOKMARK:
        case app::Command::ZOOM_IN:
        case app::Command::ZOOM_OUT:
        case app::Command::RESET_VIEW:
        case app::Command::FLIGHT:
        case app::Command::STOP_DEMO:
        case app::Command::TOUR:
        case app::Command::BACK_TO_SNAPSHOT:
            break;  // run by the app layer above, or not there yet
    }
}

void MainWindow::showAbout()
{
    QMessageBox box(this);
    box.setWindowTitle(toQt(std::format("About {}", app::kAboutName)));
    box.setTextFormat(Qt::PlainText);
    box.setText(toQt(std::format("{} {}", app::kAboutName, MANDELBROTTER_VERSION)));
    box.setInformativeText(toQt(app::kAboutDescription));
    box.setIconPixmap(windowIcon().pixmap(64, 64));
    box.exec();
}

// ---------------------------------------------------------------------------------------------------------------
// Wiring, keys and context help

void MainWindow::wirePanel()
{
    m_panel->onSettingsChanged = [this](const RenderSettings& edited) {
        m_app.panelEdited(edited);
    };
    m_panel->onPickSeedToggled = [this](bool enabled) { m_app.setPickSeedMode(enabled); };
    m_panel->onOrbitToggled    = [this](bool enabled) { m_app.setShowOrbit(enabled); };
    m_panel->onBookmarkLoad    = [this](std::size_t index) { m_app.loadBookmark(index); };
    m_panel->onBookmarkDelete  = [this](std::size_t index) { m_app.deleteBookmark(index); };
}

bool MainWindow::isMenuShortcut(const QKeyEvent& event) const
{
    // Ctrl++ may arrive with Shift (the + key) or from the keypad.
    const QKeyCombination pressed = event.keyCombination();
    const QKeyCombination bare(
        pressed.keyboardModifiers() & ~(Qt::ShiftModifier | Qt::KeypadModifier), pressed.key());
    return std::ranges::any_of(m_actions, [&](const CommandAction& entry) {
        const QKeySequence shortcut = entry.action->shortcut();
        return !shortcut.isEmpty() && entry.action->isEnabled() &&
               (shortcut == QKeySequence(pressed) || shortcut == QKeySequence(bare));
    });
}

app::HelpContext MainWindow::helpContext() const
{
    app::HelpContext context;
    const QWidget*   focus = QApplication::focusWidget();
    if (focus == nullptr)
    {
        return context;
    }
    if (focus == m_canvas || m_canvas->isAncestorOf(focus))
    {
        context.area = app::HelpContext::Area::CANVAS;
    }
    else if (const auto section = m_panel->sectionOf(focus))
    {
        context.area         = app::HelpContext::Area::PANEL;
        context.section      = section;
        context.juliaControl = m_panel->isJuliaControl(focus);
    }
    return context;
}

void MainWindow::showContextHelp()
{
    m_app.showHelpPage(app::helpPageFor(helpContext()));
}

// ---------------------------------------------------------------------------------------------------------------
// Status bar

void MainWindow::buildStatusBar()
{
    for (std::size_t i = 0; i < m_status.size(); ++i)
    {
        auto* label = new ElidedLabel(statusBar());
        statusBar()->addWidget(label, app::kStatusStretch.at(i));
        m_status.at(i) = label;
    }
}

ElidedLabel& MainWindow::statusField(app::StatusField field) const
{
    return *m_status.at(index(field));
}

void MainWindow::setStatus(app::StatusField field, const QString& text)
{
    if (field == app::StatusField::POINTER)
    {
        m_pointerText = text;
        if (m_showingTip)
        {
            return;
        }
    }
    m_status.at(index(field))->setFullText(text);
}

void MainWindow::showStatusTip(const QString& tip)
{
    m_showingTip = !tip.isEmpty();
    m_status.at(index(app::StatusField::POINTER))->setFullText(m_showingTip ? tip : m_pointerText);
}

bool MainWindow::event(QEvent* event)
{
    if (event->type() == QEvent::StatusTip)
    {
        // QMainWindow would hide every field behind QStatusBar::showMessage; wx replaced only the
        // first.
        showStatusTip(static_cast<QStatusTipEvent*>(event)->tip());
        return true;
    }
    return QMainWindow::event(event);
}

// ---------------------------------------------------------------------------------------------------------------
// Windows

void MainWindow::setSidePanelShown(bool shown)
{
    m_dock->setVisible(shown);
    if (shown)
    {
        m_dock->raise();
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_app.stopDemos();
    event->accept();
}

void MainWindow::reportError(std::string_view title, std::string_view message)
{
    QMessageBox::critical(this, toQt(title), toQt(message));
}

}  // namespace mandelbrotter::qt
