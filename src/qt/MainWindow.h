#pragma once

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QEvent>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenu>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/commands.h"
#include "Mandelbrotter/app/help_routing.h"
#include "Mandelbrotter/exporter.h"
#include "qt/ExportDialog.h"

namespace mandelbrotter::qt
{

class ElidedLabel;
class FractalCanvas;
class GlobalKeyFilter;
class HelpWindow;
class SidePanel;

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

    /// The file dialogs the window asks.
    enum class FileChoice : std::uint8_t
    {
        SAVE_IMAGE,
        EXPORT_VIEW,
        IMPORT_VIEW,
    };
    /// Stand-ins for the modal dialogs, which a test cannot answer; unset, the dialogs are shown.
    struct DialogSeams
    {
        /// The file the dialog would return; nullopt: canceled.
        std::function<std::optional<std::filesystem::path>(FileChoice)> chooseFile;
        /// The name the Add bookmark dialog would return; nullopt: canceled.
        std::function<std::optional<std::string>(std::string_view suggested)> askName;
        /// Shown instead of an error box.
        std::function<void(std::string_view title, std::string_view message)> showError;
    };
    DialogSeams dialogSeams;

    // ---- widgets
    [[nodiscard]] FractalCanvas& canvas() const noexcept { return *m_canvas; }
    /// The menu item of `command` (FLIGHT: the one of builtinFlights()[flight]); nullptr if none.
    [[nodiscard]] QAction* action(app::Command command, std::size_t flight = 0) const;
    /// Help > About Qt, which only the Qt build has.
    [[nodiscard]] QAction*     aboutQtAction() const noexcept { return m_aboutQt; }
    [[nodiscard]] ElidedLabel& statusField(app::StatusField field) const;
    [[nodiscard]] QDockWidget& panelDock() const noexcept { return *m_dock; }
    [[nodiscard]] SidePanel&   panel() const noexcept { return *m_panel; }
    /// Shows (and raises) or hides the side panel's dock; View > Show side panel follows.
    void setSidePanelShown(bool shown);
    /// Where the keyboard focus is, for F1.
    [[nodiscard]] app::HelpContext helpContext() const;
    /// True when `event` is the shortcut of one of the menu items.
    [[nodiscard]] bool isMenuShortcut(const QKeyEvent& event) const;
    /// The Save image as PNG dialog, modeless; a second call raises it.
    void showExportDialog();
    void closeExportDialog();
    /// The help window, created on first use.
    [[nodiscard]] HelpWindow& help();
    [[nodiscard]] bool        helpShown() const noexcept;
    /// The open Save image as PNG dialog, if any.
    [[nodiscard]] ExportDialog* exportDialog() const noexcept { return m_exportDialog; }

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
    void wirePanel();
    void buildStatusBar();
    void setStatus(app::StatusField field, const QString& text);
    /// A menu item's status tip covers the pointer field while it is hovered (empty: uncover).
    void showStatusTip(const QString& tip);
    void showAbout();
    /// F1: the page about whatever has the keyboard focus.
    void showContextHelp();
    void reportError(std::string_view title, std::string_view message);
    [[nodiscard]] std::optional<std::filesystem::path> chooseFile(FileChoice choice);
    void                                               saveImage(const ExportOptions& options);
    void                                               copyImage();
    void                                               exportView();
    void                                               importView();
    void                                               addBookmark();

    QTimer                     m_demoTimer;  ///< ticks the flights; stopped first on destruction
    FractalCanvas*             m_canvas{nullptr};  ///< the central widget
    QDockWidget*               m_dock{nullptr};
    SidePanel*                 m_panel{nullptr};
    GlobalKeyFilter*           m_keyFilter{nullptr};
    QPointer<ExportDialog>     m_exportDialog;
    std::vector<CommandAction> m_actions;
    QAction*                   m_aboutQt{nullptr};
    std::array<ElidedLabel*, app::kStatusFieldCount> m_status{};
    QString                                          m_pointerText;  ///< under a status tip
    bool                                             m_showingTip{false};
    app::AppController                               m_app;  ///< after the widgets its shell drives
    std::unique_ptr<HelpWindow>                      m_help;  ///< a top-level window of its own
};

}  // namespace mandelbrotter::qt
