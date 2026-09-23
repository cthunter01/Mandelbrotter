#include "gui/MainFrame.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <wx/aboutdlg.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/filedlg.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/statusbr.h>
#include <wx/textdlg.h>

#include "Mandelbrotter/ProgressiveRenderer.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/exporter.h"
#include "Mandelbrotter/flights.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/help_action.h"
#include "gui/ExportDialog.h"
#include "gui/GuidedTour.h"
#include "gui/ScreenshotRun.h"
#include "gui/SidePanel.h"
#include "gui/export_runner.h"
#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

namespace
{

enum class StatusField : std::uint8_t
{
    POINTER,
    CENTER,
    ZOOM,
    ITERATIONS,
    RENDER,
    COUNT,
};

constexpr int field(StatusField f)
{
    return static_cast<int>(f);
}

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

constexpr double kMenuZoomFactor = 2.0;

std::string_view helpPageFor(SidePanel::Section section, bool juliaControl)
{
    switch (section)
    {
        case SidePanel::Section::FRACTAL:
            return juliaControl ? "julia.html" : "fractals.html";
        case SidePanel::Section::ITERATIONS:
            return "iterations.html";
        case SidePanel::Section::COLOURING:
            return "colouring.html";
        case SidePanel::Section::OVERLAY:
            return "orbit.html";
        case SidePanel::Section::BOOKMARKS:
            return "bookmarks.html";
    }
    return "index.html";
}

}  // namespace

MainFrame::MainFrame(RenderSettings initial, std::filesystem::path bookmarksPath)
  : wxFrame(nullptr, wxID_ANY, "Mandelbrotter", wxDefaultPosition, wxDefaultSize),
    m_settings(std::move(initial)),
    m_bookmarks(std::move(bookmarksPath)),
    m_demo({.applyFrame =
                [this](const RenderSettings& frame) {
                    m_settings = frame;
                    m_canvas->setSettings(frame);
                    updateStatusBar();  // the panel catches up when the flight ends
                },
            .canvasReady = [this] { return m_canvas->hasCoarsePicture(); },
            .showStatus =
                [this](std::string_view text) {
                    SetStatusText(toWx(text), field(StatusField::POINTER));
                },
            .finished = [this] { m_panel->setSettings(m_settings); }}),
    m_canvas(new FractalCanvas(this, m_settings)),
    m_panel(new SidePanel(this))
{
    SetClientSize(FromDIP(wxSize(1280, 800)));

    auto* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(m_canvas, wxSizerFlags(1).Expand());
    sizer->Add(m_panel, wxSizerFlags().Expand());
    SetSizer(sizer);

    CreateStatusBar(field(StatusField::COUNT));
    constexpr std::array<int, field(StatusField::COUNT)> kWidths{-3, -3, -1, -1, -2};
    GetStatusBar()->SetStatusWidths(field(StatusField::COUNT), kWidths.data());

    buildMenus();
    wireCanvas();
    wirePanel();
    wireHelp();
    Bind(wxEVT_CHAR_HOOK, &MainFrame::onCharHook, this);
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::onClose, this);

    if (const std::string error = m_bookmarks.load(); !error.empty())
    {
        reportError("Bookmarks", "Could not read " + m_bookmarks.path().string() + ":\n" + error);
    }
    refreshBookmarks();
    applySettings(m_settings);
    m_canvas->SetFocus();
}

MainFrame::~MainFrame() = default;

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
        wxEVT_MENU, [this](wxCommandEvent& event) { setShowOrbit(event.IsChecked()); },
        kMenuShowOrbit);
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&) {
            stopFlight();
            m_canvas->zoomAtCenter(kMenuZoomFactor);
        },
        kMenuZoomIn);
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&) {
            stopFlight();
            m_canvas->zoomAtCenter(1.0 / kMenuZoomFactor);
        },
        kMenuZoomOut);
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&) {
            stopFlight();
            m_canvas->resetView();
        },
        kMenuResetView);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { addBookmark(); }, kMenuAddBookmark);
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
                takeSnapshot();
                startFlight(flight.id);
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
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { stopDemos(); }, kMenuStopDemo);
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&) {
            takeSnapshot();
            startTour();
        },
        kMenuTour);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { restoreSnapshot(); }, kMenuBackToView);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { showAbout(); }, wxID_ABOUT);
    Bind(
        wxEVT_UPDATE_UI,
        [this](wxUpdateUIEvent& event) { event.Enable(m_demo.playing() || tourRunning()); },
        kMenuStopDemo);
    Bind(
        wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& event) { event.Enable(m_snapshot.has_value()); },
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

void MainFrame::wireCanvas()
{
    m_canvas->onUserInput   = [this] { stopFlight(); };
    m_canvas->onViewChanged = [this](const ViewSpec& view) {
        stopFlight();
        m_settings.view = view;
        m_panel->setSettings(m_settings);
        updateStatusBar();
    };
    m_canvas->onPointerMoved = [this](const std::optional<BigComplex>& pointer) {
        showPointer(pointer);
        m_panel->setPreviewSeed(pointer ? std::optional<Complex>(pointer->approx())
                                        : std::optional<Complex>());
    };
    m_canvas->onSeedPicked = [this](Complex seed) {
        RenderSettings next = m_settings;
        next.fractal.julia  = true;
        next.fractal.seed   = seed;
        next.view           = defaultView(next.fractal);
        m_canvas->setPickSeedMode(false);
        m_panel->setPickSeedMode(false);
        applySettings(next);
    };
    m_canvas->onRenderStatus = [this](const FractalCanvas::RenderStatus& status) {
        showRenderStatus(status);
    };
}

void MainFrame::wirePanel()
{
    m_panel->onSettingsChanged = [this](const RenderSettings& edited) {
        stopFlight();
        RenderSettings next = edited;
        next.view           = m_settings.view;  // the panel never edits the view
        if (next.fractal.family != m_settings.fractal.family ||
            next.fractal.julia != m_settings.fractal.julia)
        {
            next.view = defaultView(next.fractal);
        }
        applySettings(next);
    };
    m_panel->onPickSeedToggled = [this](bool enabled) { m_canvas->setPickSeedMode(enabled); };
    m_panel->onOrbitToggled    = [this](bool enabled) { setShowOrbit(enabled); };
    m_panel->onBookmarkAdd     = [this] { addBookmark(); };
    m_panel->onBookmarkLoad    = [this](std::size_t index) { loadBookmark(index); };
    m_panel->onBookmarkDelete  = [this](std::size_t index) { deleteBookmark(index); };
}

void MainFrame::wireHelp()
{
    m_help.onAction = [this](const HelpAction& action) { runHelpAction(action); };
    m_help.onError  = [this](const std::string& message) { reportError("Help", message); };
}

void MainFrame::onCharHook(wxKeyEvent& event)
{
    const bool demoRunning = m_demo.playing() || tourRunning();
    if (event.GetKeyCode() == WXK_ESCAPE && demoRunning)
    {
        stopDemos();
        return;
    }
    if (m_demo.playing())
    {
        stopFlight();  // any key ends a flight; the key then does its usual job
    }
    event.Skip();
}

void MainFrame::onClose(wxCloseEvent& event)
{
    stopDemos();
    closeExportDialog();
    event.Skip();
}

// ---------------------------------------------------------------------------------------------------------------
// Model

void MainFrame::applySettings(const RenderSettings& settings)
{
    m_settings = settings;
    m_canvas->setSettings(settings);
    m_panel->setSettings(settings);
    updateStatusBar();
}

void MainFrame::setShowOrbit(bool on)
{
    m_canvas->setShowOrbit(on);
    m_panel->setShowOrbit(on);
    m_showOrbitItem->Check(on);
}

void MainFrame::setSidePanelShown(bool shown)
{
    m_showPanelItem->Check(shown);
    GetSizer()->Show(m_panel, shown);
    Layout();
}

void MainFrame::updateStatusBar()
{
    SetStatusText(toWx("Centre " + formatCenter(m_settings.view.center, m_settings.view.zoom)),
                  field(StatusField::CENTER));
    const std::string zoomText =
        formatZoom(m_settings.view.zoom) +
        (usesPerturbation(m_settings.view.zoom) ? " (deep)" : "");  // perturbation rendering
    SetStatusText(toWx("Zoom " + zoomText), field(StatusField::ZOOM));
    SetStatusText(toWx(std::format("{} iterations", effectiveIterations(m_settings))),
                  field(StatusField::ITERATIONS));
    m_panel->setEffectiveIterations(effectiveIterations(m_settings));
}

void MainFrame::showPointer(const std::optional<BigComplex>& pointer)
{
    if (m_demo.playing())
    {
        return;  // the field shows the flight's progress
    }
    SetStatusText(pointer ? toWx(formatCenter(*pointer, m_settings.view.zoom)) : wxString(),
                  field(StatusField::POINTER));
}

void MainFrame::showRenderStatus(const FractalCanvas::RenderStatus& status)
{
    if (status.rendering)
    {
        SetStatusText("Rendering...", field(StatusField::RENDER));
        return;
    }
    SetStatusText(toWx(std::format("Rendered in {} ms", status.elapsed.count())),
                  field(StatusField::RENDER));
    if (onRenderFinished)
    {
        onRenderFinished();
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Help and demos

GuidedTour& MainFrame::tour()
{
    if (!m_tour)
    {
        m_tour = std::make_unique<GuidedTour>(*this);
    }
    return *m_tour;
}

wxWindow* MainFrame::exportDialog() noexcept
{
    return m_exportDialog;
}

void MainFrame::showHelpPage(std::string_view page)
{
    m_help.showPage(page);
}

void MainFrame::showContextHelp()
{
    std::string_view page  = "index.html";
    wxWindow*        focus = wxWindow::FindFocus();
    if (focus != nullptr)
    {
        if (focus == m_canvas || m_canvas->IsDescendant(focus))
        {
            page = "navigating.html";
        }
        else if (const auto section = m_panel->sectionOf(focus))
        {
            page = helpPageFor(*section, m_panel->isJuliaControl(focus));
        }
        else if (m_exportDialog != nullptr &&
                 (focus == m_exportDialog || m_exportDialog->IsDescendant(focus)))
        {
            page = "exporting.html";
        }
    }
    m_help.showPage(page);
}

void MainFrame::runHelpAction(const HelpAction& action)
{
    if (const auto* view = std::get_if<ViewAction>(&action))
    {
        const auto next = resolveViewAction(m_settings, *view);
        if (!next)
        {
            reportError("Try it", next.error());
            return;
        }
        takeSnapshot();
        stopFlight();
        applySettings(*next);
    }
    else if (const auto* flight = std::get_if<FlightAction>(&action))
    {
        takeSnapshot();
        startFlight(flight->id);
    }
    else if (std::holds_alternative<TourAction>(action))
    {
        takeSnapshot();
        startTour();
    }
    else if (const auto* orbit = std::get_if<OrbitAction>(&action))
    {
        takeSnapshot();
        setShowOrbit(orbit->on);
    }
    else if (std::holds_alternative<ResetAction>(action))
    {
        takeSnapshot();
        stopFlight();
        m_canvas->resetView();
    }
    else if (std::holds_alternative<ExportDialogAction>(action))
    {
        showExportDialog();
    }
    Raise();
}

void MainFrame::startFlight(std::string_view id)
{
    const Flight* flight = findFlight(id);
    if (flight == nullptr)
    {
        reportError("Demos", "There is no flight called \"" + std::string(id) + "\".");
        return;
    }
    if (m_tour)
    {
        m_tour->stop();
    }
    m_demo.play(*flight);
}

void MainFrame::startTour()
{
    stopFlight();
    tour().start();
}

void MainFrame::stopFlight()
{
    m_demo.stop();
}

void MainFrame::stopDemos()
{
    stopFlight();
    if (m_tour)
    {
        m_tour->stop();
    }
}

bool MainFrame::tourRunning() const noexcept
{
    return m_tour && m_tour->running();
}

void MainFrame::takeSnapshot()
{
    if (m_snapshot && (m_demo.playing() || tourRunning()))
    {
        return;  // keep the view from before the demo that is running
    }
    m_snapshot = Snapshot{.settings = m_settings, .showOrbit = m_canvas->showOrbit()};
}

void MainFrame::restoreSnapshot()
{
    if (!m_snapshot)
    {
        return;
    }
    const Snapshot snapshot = *m_snapshot;
    m_snapshot.reset();
    stopDemos();
    applySettings(snapshot.settings);
    setShowOrbit(snapshot.showOrbit);
}

void MainFrame::addTemporaryBookmark(std::string name)
{
    removeTemporaryBookmark();
    Bookmark bookmark{.name = std::move(name), .settings = m_settings};
    m_temporaryBookmark = bookmark;
    m_bookmarks.add(std::move(bookmark));
    refreshBookmarks();
}

void MainFrame::removeTemporaryBookmark()
{
    if (!m_temporaryBookmark)
    {
        return;
    }
    const auto& list = m_bookmarks.bookmarks();
    for (std::size_t i = list.size(); i-- > 0;)
    {
        if (list[i] == *m_temporaryBookmark)
        {
            m_bookmarks.remove(i);
            break;
        }
    }
    m_temporaryBookmark.reset();
    refreshBookmarks();
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
    m_exportDialog         = new ExportDialog(this, m_canvas->currentImage().size());
    m_exportDialog->onHelp = [this] { showHelpPage("exporting.html"); };
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
    if (exportPngWithProgress(this, m_settings, options, path))
    {
        SetStatusText(toWx("Saved " + path.filename().string()), field(StatusField::RENDER));
    }
}

void MainFrame::copyImage()
{
    const RgbImage& image = m_canvas->currentImage();
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
    SetStatusText("Image copied to clipboard", field(StatusField::RENDER));
}

void MainFrame::exportView()
{
    wxFileDialog chooser(this, "Export view", "", "view.json",
                         "Mandelbrotter views (*.json)|*.json", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (chooser.ShowModal() != wxID_OK)
    {
        return;
    }
    try
    {
        saveView(std::filesystem::path(fromWx(chooser.GetPath())), m_settings);
    }
    catch (const std::exception& e)
    {
        reportError("Export view", e.what());
    }
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
    try
    {
        stopFlight();
        applySettings(loadView(std::filesystem::path(fromWx(chooser.GetPath()))));
    }
    catch (const std::exception& e)
    {
        reportError("Import view", e.what());
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Bookmarks

void MainFrame::addBookmark()
{
    stopDemos();  // the tour's temporary bookmark must never be saved
    wxTextEntryDialog dialog(this, "Name for this view:", "Add bookmark",
                             toWx(std::string(displayName(m_settings.fractal.family)) + " at " +
                                  formatZoom(m_settings.view.zoom)));
    if (dialog.ShowModal() != wxID_OK)
    {
        return;
    }
    std::string name = fromWx(dialog.GetValue());
    if (name.empty())
    {
        name = "Untitled";
    }
    m_bookmarks.add({.name = std::move(name), .settings = m_settings});
    saveBookmarksFile();
    refreshBookmarks();
}

void MainFrame::loadBookmark(std::size_t index)
{
    stopFlight();
    const auto& list = m_bookmarks.bookmarks();
    if (index < list.size())
    {
        applySettings(list[index].settings);
    }
}

void MainFrame::deleteBookmark(std::size_t index)
{
    if (tourRunning())
    {
        stopDemos();  // removes the tour's example; the list has changed under the selection
        return;
    }
    stopFlight();
    m_bookmarks.remove(index);
    saveBookmarksFile();
    refreshBookmarks();
}

void MainFrame::saveBookmarksFile()
{
    if (const std::string error = m_bookmarks.save(); !error.empty())
    {
        reportError("Bookmarks", "Could not write " + m_bookmarks.path().string() + ":\n" + error);
    }
}

void MainFrame::refreshBookmarks()
{
    m_panel->setBookmarks(m_bookmarks.bookmarks());
}

void MainFrame::reportError(const std::string& title, const std::string& message)
{
    wxMessageBox(toWx(message), toWx(title), wxOK | wxICON_ERROR, this);
}

}  // namespace mandelbrotter::gui
