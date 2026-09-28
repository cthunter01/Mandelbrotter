#pragma once

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QEvent>
#include <QMainWindow>
#include <QMenu>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <array>
#include <cstddef>
#include <filesystem>
#include <string_view>
#include <vector>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/commands.h"

namespace mandelbrotter::qt
{

class ElidedLabel;
class FractalCanvas;

/// The main window: the Qt side of app::AppController, which holds the model and the policies.
/// The window builds the widgets, menus and dialogs and routes their events to app(); app() and the
/// few window operations below are what the help system and the tests drive.
class MainWindow : public QMainWindow
{
public:
    /// `bookmarksPath` is the bookmarks file this window reads and writes.
    MainWindow(RenderSettings initial, std::filesystem::path bookmarksPath);
    ~MainWindow() override;
    MainWindow(const MainWindow&)            = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    MainWindow(MainWindow&&)                 = delete;
    MainWindow& operator=(MainWindow&&)      = delete;

    [[nodiscard]] app::AppController& app() noexcept { return m_app; }

    // ---- widgets
    [[nodiscard]] FractalCanvas& canvas() const noexcept { return *m_canvas; }
    /// The menu item of `command` (FLIGHT: the one of builtinFlights()[flight]); nullptr if none.
    [[nodiscard]] QAction* action(app::Command command, std::size_t flight = 0) const;
    /// Help > About Qt, which only the Qt build has.
    [[nodiscard]] QAction*     aboutQtAction() const noexcept { return m_aboutQt; }
    [[nodiscard]] ElidedLabel& statusField(app::StatusField field) const;
    [[nodiscard]] QDockWidget& panelDock() const noexcept { return *m_dock; }
    void                       setSidePanelShown(bool shown);

protected:
    bool event(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    /// A menu item and the command it runs.
    struct CommandAction
    {
        app::Command    command{};
        std::size_t     flight{};
        app::EnableRule enable{app::EnableRule::ALWAYS};
        QAction*        action{nullptr};
    };

    [[nodiscard]] app::AppController::Shell makeShell();
    void                                    buildMenus();
    void addMenuEntries(QMenu& menu, const std::vector<app::MenuEntry>& entries);
    void addCommandAction(QMenu& menu, const app::MenuEntry& entry);
    /// A menu item was chosen: the app layer's commands first, then the ones that need Qt.
    void runCommand(app::Command command, std::size_t flight, bool checked);
    /// Stop demo and Back to where I was follow the app's state when their menu opens.
    void refreshEnabledActions();
    void buildStatusBar();
    void setStatus(app::StatusField field, const QString& text);
    /// A menu item's status tip covers the pointer field while it is hovered (empty: uncover).
    void showStatusTip(const QString& tip);
    void showAbout();
    void reportError(std::string_view title, std::string_view message);

    QTimer                     m_demoTimer;  ///< ticks the flights; stopped first on destruction
    FractalCanvas*             m_canvas{nullptr};  ///< the central widget
    QDockWidget*               m_dock{nullptr};
    std::vector<CommandAction> m_actions;
    QAction*                   m_aboutQt{nullptr};
    std::array<ElidedLabel*, app::kStatusFieldCount> m_status{};
    QString                                          m_pointerText;  ///< under a status tip
    bool                                             m_showingTip{false};
    app::AppController                               m_app;  ///< after the widgets its shell drives
};

}  // namespace mandelbrotter::qt
