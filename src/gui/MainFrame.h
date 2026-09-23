#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <wx/frame.h>
#include <wx/menu.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/exporter.h"
#include "Mandelbrotter/help_action.h"
#include "gui/BookmarkStore.h"
#include "gui/DemoPlayer.h"
#include "gui/FractalCanvas.h"
#include "gui/HelpController.h"

namespace mandelbrotter::gui
{

class ExportDialog;
class GuidedTour;
class ScreenshotRun;
class SidePanel;

/// The main window. The public methods beyond the constructor are what the help system drives:
/// Try-it links, the demo flights, the guided tour and the screenshot mode all act through them.
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

    // ---- the model
    /// Makes `settings` the current model everywhere (canvas, panel, status bar).
    void                                applySettings(const RenderSettings& settings);
    [[nodiscard]] const RenderSettings& settings() const noexcept { return m_settings; }
    void                                setShowOrbit(bool on);
    void                                setSidePanelShown(bool shown);

    // ---- windows
    [[nodiscard]] FractalCanvas&               canvas() noexcept { return *m_canvas; }
    [[nodiscard]] SidePanel&                   panel() noexcept { return *m_panel; }
    [[nodiscard]] HelpController&              help() noexcept { return m_help; }
    [[nodiscard]] GuidedTour&                  tour();
    [[nodiscard]] wxWindow*                    exportDialog() noexcept;
    [[nodiscard]] const std::filesystem::path& bookmarksPath() const noexcept
    {
        return m_bookmarks.path();
    }
    /// The Save image as PNG dialog, modeless; a second call raises it.
    void showExportDialog();
    void closeExportDialog();
    void showHelpPage(std::string_view page);
    /// F1: the page about whatever has the keyboard focus.
    void showContextHelp();

    // ---- demos
    void startFlight(std::string_view id);
    void startTour();
    /// Stops a flight and the tour (the tour restores what the user had).
    void stopDemos();
    /// Remembers the view for Help > Back to where I was; ignored while a demo already runs.
    void takeSnapshot();
    void restoreSnapshot();
    /// A bookmark that only appears in the list (the tour's example); never written to disk.
    void addTemporaryBookmark(std::string name);
    void removeTemporaryBookmark();

    // ---- developer mode: --screenshots DIR
    void startScreenshotRun(const std::filesystem::path& dir);
    /// Called after every render that ran to completion.
    std::function<void()> onRenderFinished;

private:
    struct Snapshot
    {
        RenderSettings settings;
        bool           showOrbit{};
    };

    void buildMenus();
    void buildHelpMenu(wxMenu& help);
    void wireCanvas();
    void wirePanel();
    void wireHelp();

    void updateStatusBar();
    void showPointer(const std::optional<BigComplex>& pointer);
    void showRenderStatus(const FractalCanvas::RenderStatus& status);
    void onCharHook(wxKeyEvent& event);
    void onClose(wxCloseEvent& event);

    void               runHelpAction(const HelpAction& action);
    void               stopFlight();
    [[nodiscard]] bool tourRunning() const noexcept;
    void               showAbout();

    void saveImage(const ExportOptions& options);
    void copyImage();
    void exportView();
    void importView();
    void addBookmark();
    void loadBookmark(std::size_t index);
    void deleteBookmark(std::size_t index);
    void saveBookmarksFile();
    void refreshBookmarks();
    void reportError(const std::string& title, const std::string& message);

    RenderSettings                 m_settings;
    BookmarkStore                  m_bookmarks;
    HelpController                 m_help;
    DemoPlayer                     m_demo;
    std::unique_ptr<GuidedTour>    m_tour;
    std::unique_ptr<ScreenshotRun> m_screenshots;
    FractalCanvas*                 m_canvas{nullptr};
    SidePanel*                     m_panel{nullptr};
    ExportDialog*                  m_exportDialog{nullptr};
    wxMenuItem*                    m_showPanelItem{nullptr};
    wxMenuItem*                    m_showOrbitItem{nullptr};
    std::optional<Snapshot>        m_snapshot;
    std::optional<Bookmark>        m_temporaryBookmark;
};

}  // namespace mandelbrotter::gui
