#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <wx/frame.h>
#include <wx/menu.h>
#include <wx/timer.h>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/commands.h"
#include "Mandelbrotter/exporter.h"
#include "gui/FractalCanvas.h"
#include "gui/HelpController.h"
#include "gui/TourView.h"

namespace mandelbrotter::gui
{

class ExportDialog;
class ScreenshotRun;
class SidePanel;

/// The main window: the wx side of app::AppController, which holds the model and the policies.
/// The frame builds the widgets, menus and dialogs and routes their events to app(); app() and the
/// few window operations below are what the help system drives (Try-it links, the demos, the tour
/// and the screenshot mode).
class MainFrame : public wxFrame
{
public:
    /// `bookmarksPath` is the bookmarks file this window reads and writes.
    MainFrame(RenderSettings initial, std::filesystem::path bookmarksPath);
    ~MainFrame() override;
    MainFrame(const MainFrame&)            = delete;
    MainFrame& operator=(const MainFrame&) = delete;
    MainFrame(MainFrame&&)                 = delete;
    MainFrame& operator=(MainFrame&&)      = delete;

    [[nodiscard]] app::AppController& app() noexcept { return m_app; }

    // ---- windows
    [[nodiscard]] FractalCanvas&  canvas() noexcept { return *m_canvas; }
    [[nodiscard]] SidePanel&      panel() noexcept { return *m_panel; }
    [[nodiscard]] HelpController& help() noexcept { return m_help; }
    [[nodiscard]] wxWindow*       exportDialog() noexcept;
    void                          setSidePanelShown(bool shown);
    /// The Save image as PNG dialog, modeless; a second call raises it.
    void showExportDialog();
    void closeExportDialog();
    /// F1: the page about whatever has the keyboard focus.
    void showContextHelp();

    // ---- developer mode: --screenshots DIR
    void startScreenshotRun(const std::filesystem::path& dir);

private:
    [[nodiscard]] app::AppController::Shell makeShell();
    void                                    buildMenus();
    /// Appends `entries` (app::menuBar()) to `menu`; `nextId` numbers the items without a stock ID.
    void appendMenuEntries(wxMenu& menu, const std::vector<app::MenuEntry>& entries, int& nextId);
    /// A menu item was chosen: the app layer's commands first, then the ones that need wx.
    void runCommand(app::Command command, std::size_t flight, bool checked);
    void wirePanel();
    void wireHelp();
    void onCharHook(wxKeyEvent& event);
    void onClose(wxCloseEvent& event);
    void showAbout();

    void saveImage(const ExportOptions& options);
    void copyImage();
    void exportView();
    void importView();
    void onAddBookmark();
    void reportError(const std::string& title, const std::string& message);

    HelpController     m_help;
    wxTimer            m_demoTimer;  ///< ticks the flights; stopped first on destruction
    FractalCanvas*     m_canvas{nullptr};
    SidePanel*         m_panel{nullptr};
    app::AppController m_app;  ///< after the widgets its shell drives
    TourView           m_tourView;
    std::unique_ptr<ScreenshotRun> m_screenshots;
    ExportDialog*                  m_exportDialog{nullptr};
    wxMenuItem*                    m_showPanelItem{nullptr};
    wxMenuItem*                    m_showOrbitItem{nullptr};
};

}  // namespace mandelbrotter::gui
