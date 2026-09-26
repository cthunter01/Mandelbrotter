#include "gui/MainFrame.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <wx/aboutdlg.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/filedlg.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/statusbr.h>
#include <wx/textdlg.h>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/DemoPlayer.h"
#include "Mandelbrotter/app/TourScript.h"
#include "Mandelbrotter/app/help_routing.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/exporter.h"
#include "Mandelbrotter/flights.h"
#include "Mandelbrotter/help_action.h"
#include "gui/ExportDialog.h"
#include "gui/ScreenshotRun.h"
#include "gui/SidePanel.h"
#include "gui/app_icon.h"
#include "gui/export_runner.h"
#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

namespace
{

constexpr int kMenuSaveImage   = wxID_HIGHEST + 1;
constexpr int kMenuCopyImage   = wxID_HIGHEST + 2;
constexpr int kMenuExportView  = wxID_HIGHEST + 3;
constexpr int kMenuImportView  = wxID_HIGHEST + 4;
constexpr int kMenuShowPanel   = wxID_HIGHEST + 5;
constexpr int kMenuShowOrbit   = wxID_HIGHEST + 6;
constexpr int kMenuResetView   = wxID_HIGHEST + 7;
constexpr int kMenuZoomIn      = wxID_HIGHEST + 8;
constexpr int kMenuZoomOut     = wxID_HIGHEST + 9;
constexpr int kMenuAddBookmark = wxID_HIGHEST + 10;
constexpr int kMenuContextHelp = wxID_HIGHEST + 11;
constexpr int kMenuReference   = wxID_HIGHEST + 12;
constexpr int kMenuTour        = wxID_HIGHEST + 13;
constexpr int kMenuStopDemo    = wxID_HIGHEST + 14;
constexpr int kMenuBackToView  = wxID_HIGHEST + 15;
/// One item per built-in flight, in order.
constexpr int kMenuFlightFirst = wxID_HIGHEST + 100;

constexpr int field(app::StatusField f)
{
    return static_cast<int>(f);
}

}  // namespace

MainFrame::MainFrame(RenderSettings initial, std::filesystem::path bookmarksPath)
  : wxFrame(nullptr, wxID_ANY, "Mandelbrotter", wxDefaultPosition, wxDefaultSize),
    m_demoTimer(this),
    m_canvas(new FractalCanvas(this, initial)),
    m_panel(new SidePanel(this)),
    m_app(std::move(initial), std::move(bookmarksPath), m_canvas->controller(), makeShell()),
    m_tourView(*this)
{
    applyAppIcon(*this);
    SetClientSize(FromDIP(wxSize(1280, 800)));

    auto* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(m_canvas, wxSizerFlags(1).Expand());
    sizer->Add(m_panel, wxSizerFlags().Expand());
    SetSizer(sizer);

    CreateStatusBar(app::kStatusFieldCount);
    constexpr std::array<int, app::kStatusFieldCount> kWidths{-3, -3, -1, -1, -2};
    GetStatusBar()->SetStatusWidths(app::kStatusFieldCount, kWidths.data());

    buildMenus();
    wirePanel();
    wireHelp();
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { m_app.tickDemo(); }, m_demoTimer.GetId());
    Bind(wxEVT_CHAR_HOOK, &MainFrame::onCharHook, this);
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::onClose, this);

    m_app.start();
    m_canvas->SetFocus();
}

MainFrame::~MainFrame()
{
    m_demoTimer.Stop();
    // The panel's controls outlive the frame's members; nothing they report may reach m_app now.
    m_panel->onSettingsChanged = nullptr;
    m_panel->onPickSeedToggled = nullptr;
    m_panel->onOrbitToggled    = nullptr;
    m_panel->onBookmarkAdd     = nullptr;
    m_panel->onBookmarkLoad    = nullptr;
    m_panel->onBookmarkDelete  = nullptr;
}

app::AppController::Shell MainFrame::makeShell()
{
    // Called before m_app exists; the hooks only run once it does.
    return {
        .setStatus = [this](app::StatusField f,
                            std::string_view text) { SetStatusText(toWx(text), field(f)); },
        .reportError =
            [this](std::string_view title, std::string_view message) {
                reportError(std::string(title), std::string(message));
            },
        .panelSettings = [this](const RenderSettings& settings) { m_panel->setSettings(settings); },
        .panelEffectiveIterations =
            [this](int iterations) { m_panel->setEffectiveIterations(iterations); },
        .panelBookmarks = [this](std::span<const Bookmark> list) { m_panel->setBookmarks(list); },
        .panelPickSeedMode = [this](bool on) { m_panel->setPickSeedMode(on); },
        .showOrbitChanged =
            [this](bool on) {
                m_panel->setShowOrbit(on);
                m_showOrbitItem->Check(on);
            },
        .panelPreviewSeed  = [this](std::optional<Complex> seed) { m_panel->setPreviewSeed(seed); },
        .showExportDialog  = [this] { showExportDialog(); },
        .closeExportDialog = [this] { closeExportDialog(); },
        .raiseWindow       = [this] { Raise(); },
        .showHelpPage      = [this](std::string_view page) { m_help.showPage(page); },
        .demoTimer =
            [this](bool on) {
                if (on)
                {
                    m_demoTimer.Start(static_cast<int>(app::kDemoTick.count()));
                }
                else
                {
                    m_demoTimer.Stop();
                }
            },
        .now  = {},
        .tour = {.showCard = [this](const app::TourStep& step, std::size_t index,
                                    std::size_t count) { m_tourView.showCard(step, index, count); },
                 .hideCard = [this] { m_tourView.hideCard(); },
                 .highlight =
                     [this](std::optional<app::TourTarget> target) {
                         m_tourView.highlight(target);
                     }},
    };
}

// ---------------------------------------------------------------------------------------------------------------
// Menus

void MainFrame::buildMenus()
{
    auto* file = new wxMenu();
    file->Append(kMenuSaveImage, "&Save image as PNG...\tCtrl+S");
    file->Append(kMenuCopyImage, "&Copy image\tCtrl+C");
    file->AppendSeparator();
    file->Append(kMenuExportView, "&Export view...", "Save the current view as a JSON file");
    file->Append(kMenuImportView, "&Import view...", "Open a view saved as JSON");
    file->AppendSeparator();
    file->Append(wxID_EXIT, "&Quit\tCtrl+Q");

    auto* view      = new wxMenu();
    m_showPanelItem = view->AppendCheckItem(kMenuShowPanel, "Show &side panel\tCtrl+B");
    m_showPanelItem->Check(true);
    m_showOrbitItem = view->AppendCheckItem(kMenuShowOrbit, "Show &orbit under cursor\tCtrl+O");
    view->AppendSeparator();
    view->Append(kMenuZoomIn, "Zoom &in\tCtrl++");
    view->Append(kMenuZoomOut, "Zoom &out\tCtrl+-");
    view->Append(kMenuResetView, "&Reset view\tCtrl+Home");

    auto* bookmarks = new wxMenu();
    bookmarks->Append(kMenuAddBookmark, "&Add bookmark...\tCtrl+D");

    auto* help = new wxMenu();
    buildHelpMenu(*help);

    auto* bar = new wxMenuBar();
    bar->Append(file, "&File");
    bar->Append(view, "&View");
    bar->Append(bookmarks, "&Bookmarks");
    bar->Append(help, "&Help");
    SetMenuBar(bar);

    Bind(wxEVT_MENU, [this](wxCommandEvent&) { showExportDialog(); }, kMenuSaveImage);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { copyImage(); }, kMenuCopyImage);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { exportView(); }, kMenuExportView);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { importView(); }, kMenuImportView);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(true); }, wxID_EXIT);
    Bind(
        wxEVT_MENU, [this](wxCommandEvent& event) { setSidePanelShown(event.IsChecked()); },
        kMenuShowPanel);
    Bind(
        wxEVT_MENU, [this](wxCommandEvent& event) { m_app.setShowOrbit(event.IsChecked()); },
        kMenuShowOrbit);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { m_app.zoomIn(); }, kMenuZoomIn);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { m_app.zoomOut(); }, kMenuZoomOut);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { m_app.resetView(); }, kMenuResetView);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onAddBookmark(); }, kMenuAddBookmark);
}

void MainFrame::buildHelpMenu(wxMenu& help)
{
    help.Append(wxID_HELP_CONTENTS, "&Contents", "Open the user guide");
    help.Append(kMenuContextHelp, "Help for the &focused control\tF1",
                "Open the page about the control that has the keyboard focus");
    help.Append(kMenuReference, "Keyboard and mouse &reference");
    help.AppendSeparator();

    auto* demos = new wxMenu();
    int   id    = kMenuFlightFirst;
    for (const Flight& flight : builtinFlights())
    {
        demos->Append(id, toWx(flight.title), toWx(flight.description));
        Bind(
            wxEVT_MENU,
            [this, &flight](wxCommandEvent&) {
                m_app.takeSnapshot();
                m_app.startFlight(flight.id);
            },
            id);
        ++id;
    }
    demos->AppendSeparator();
    demos->Append(kMenuStopDemo, "&Stop demo", "Stop the flight or the tour");
    help.AppendSubMenu(demos, "&Demos", "Animated dives into famous places");
    help.Append(kMenuTour, "Take a &tour", "A guided walk through the window, step by step");
    help.Append(kMenuBackToView, "&Back to where I was",
                "Return to the view from before the last demo");
    help.AppendSeparator();
    help.Append(wxID_ABOUT, "&About Mandelbrotter");

    Bind(wxEVT_MENU, [this](wxCommandEvent&) { m_help.showContents(); }, wxID_HELP_CONTENTS);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { showContextHelp(); }, kMenuContextHelp);
    Bind(
        wxEVT_MENU, [this](wxCommandEvent&) { m_help.showPage("reference.html"); }, kMenuReference);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { m_app.stopDemos(); }, kMenuStopDemo);
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&) {
            m_app.takeSnapshot();
            m_app.startTour();
        },
        kMenuTour);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { m_app.restoreSnapshot(); }, kMenuBackToView);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { showAbout(); }, wxID_ABOUT);
    Bind(
        wxEVT_UPDATE_UI,
        [this](wxUpdateUIEvent& event) {
            event.Enable(m_app.flightPlaying() || m_app.tourRunning());
        },
        kMenuStopDemo);
    Bind(
        wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& event) { event.Enable(m_app.hasSnapshot()); },
        kMenuBackToView);
}

void MainFrame::showAbout()
{
    wxAboutDialogInfo info;
    info.SetName("Mandelbrotter");
    info.SetVersion(MANDELBROTTER_VERSION);
    info.SetDescription(
        "Interactive Mandelbrot-family fractal explorer.\n\n"
        "The user guide is under Help > Contents; F1 explains the focused control.");
    wxAboutBox(info, this);
}

// ---------------------------------------------------------------------------------------------------------------
// Wiring

void MainFrame::wirePanel()
{
    m_panel->onSettingsChanged = [this](const RenderSettings& edited) {
        m_app.panelEdited(edited);
    };
    m_panel->onPickSeedToggled = [this](bool enabled) { m_app.setPickSeedMode(enabled); };
    m_panel->onOrbitToggled    = [this](bool enabled) { m_app.setShowOrbit(enabled); };
    m_panel->onBookmarkAdd     = [this] { onAddBookmark(); };
    m_panel->onBookmarkLoad    = [this](std::size_t index) { m_app.loadBookmark(index); };
    m_panel->onBookmarkDelete  = [this](std::size_t index) { m_app.deleteBookmark(index); };
}

void MainFrame::wireHelp()
{
    m_help.onAction = [this](const HelpAction& action) { m_app.runHelpAction(action); };
    m_help.onError  = [this](const std::string& message) { reportError("Help", message); };
}

void MainFrame::onCharHook(wxKeyEvent& event)
{
    const app::GlobalKey key =
        event.GetKeyCode() == WXK_ESCAPE ? app::GlobalKey::ESCAPE : app::GlobalKey::OTHER;
    if (m_app.handleGlobalKey(key))
    {
        return;
    }
    event.Skip();
}

void MainFrame::onClose(wxCloseEvent& event)
{
    m_app.stopDemos();
    closeExportDialog();
    event.Skip();
}

// ---------------------------------------------------------------------------------------------------------------
// Windows

void MainFrame::setSidePanelShown(bool shown)
{
    m_showPanelItem->Check(shown);
    GetSizer()->Show(m_panel, shown);
    Layout();
}

wxWindow* MainFrame::exportDialog() noexcept
{
    return m_exportDialog;
}

void MainFrame::showContextHelp()
{
    app::HelpContext context;
    wxWindow*        focus = wxWindow::FindFocus();
    if (focus != nullptr)
    {
        if (focus == m_canvas || m_canvas->IsDescendant(focus))
        {
            context.area = app::HelpContext::Area::CANVAS;
        }
        else if (const auto section = m_panel->sectionOf(focus))
        {
            context.area         = app::HelpContext::Area::PANEL;
            context.section      = section;
            context.juliaControl = m_panel->isJuliaControl(focus);
        }
        else if (m_exportDialog != nullptr &&
                 (focus == m_exportDialog || m_exportDialog->IsDescendant(focus)))
        {
            context.area = app::HelpContext::Area::EXPORT_DIALOG;
        }
    }
    m_help.showPage(app::helpPageFor(context));
}

void MainFrame::startScreenshotRun(const std::filesystem::path& dir)
{
    m_screenshots = std::make_unique<ScreenshotRun>(*this, dir);
    m_screenshots->start();
}

// ---------------------------------------------------------------------------------------------------------------
// File actions

void MainFrame::showExportDialog()
{
    if (m_exportDialog != nullptr)
    {
        m_exportDialog->Raise();
        return;
    }
    m_exportDialog         = new ExportDialog(this, m_app.canvas().image().size());
    m_exportDialog->onHelp = [this] { m_help.showPage("exporting.html"); };
    m_exportDialog->Bind(
        wxEVT_BUTTON,
        [this](wxCommandEvent&) {
            const ExportOptions options = m_exportDialog->options();
            closeExportDialog();
            saveImage(options);
        },
        wxID_OK);
    m_exportDialog->Bind(
        wxEVT_BUTTON, [this](wxCommandEvent&) { closeExportDialog(); }, wxID_CANCEL);
    m_exportDialog->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent&) { closeExportDialog(); });
    m_exportDialog->Show();
}

void MainFrame::closeExportDialog()
{
    if (m_exportDialog == nullptr)
    {
        return;
    }
    ExportDialog* dialog = m_exportDialog;
    m_exportDialog       = nullptr;
    dialog->Destroy();
}

void MainFrame::saveImage(const ExportOptions& options)
{
    wxFileDialog chooser(this, "Save image as PNG", "", "mandelbrotter.png",
                         "PNG images (*.png)|*.png", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (chooser.ShowModal() != wxID_OK)
    {
        return;
    }
    const std::filesystem::path path(fromWx(chooser.GetPath()));
    if (exportPngWithProgress(this, m_app.settings(), options, path))
    {
        m_app.imageSaved(path);
    }
}

void MainFrame::copyImage()
{
    const RgbImage& image = m_app.canvas().image();
    if (image.size().empty())
    {
        return;
    }
    const wxClipboardLocker locker;
    if (!locker)
    {
        reportError("Copy image", "The clipboard is busy.");
        return;
    }
    // The clipboard takes ownership of the data object.
    wxTheClipboard->SetData(new wxBitmapDataObject(wxBitmap(toWxImage(image))));
    wxTheClipboard->Flush();  // keep the image available after this window closes
    m_app.imageCopied();
}

void MainFrame::exportView()
{
    wxFileDialog chooser(this, "Export view", "", "view.json",
                         "Mandelbrotter views (*.json)|*.json", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (chooser.ShowModal() != wxID_OK)
    {
        return;
    }
    m_app.exportView(std::filesystem::path(fromWx(chooser.GetPath())));
}

void MainFrame::importView()
{
    wxFileDialog chooser(this, "Import view", "", "",
                         "Mandelbrotter views (*.json)|*.json|All files|*",
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (chooser.ShowModal() != wxID_OK)
    {
        return;
    }
    m_app.importView(std::filesystem::path(fromWx(chooser.GetPath())));
}

// ---------------------------------------------------------------------------------------------------------------
// Bookmarks

void MainFrame::onAddBookmark()
{
    // Ends the demos first: the suggested name is for the user's view, not the tour's.
    const std::string suggested = m_app.beginAddBookmark();
    wxTextEntryDialog dialog(this, "Name for this view:", "Add bookmark", toWx(suggested));
    if (dialog.ShowModal() != wxID_OK)
    {
        return;
    }
    m_app.addBookmark(fromWx(dialog.GetValue()));
}

void MainFrame::reportError(const std::string& title, const std::string& message)
{
    wxMessageBox(toWx(message), toWx(title), wxOK | wxICON_ERROR, this);
}

}  // namespace mandelbrotter::gui
