#include "gui/main_frame.h"

#include <array>
#include <cstdint>
#include <exception>
#include <format>
#include <string>
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

#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/fractal.h"
#include "gui/export_dialog.h"
#include "gui/export_runner.h"
#include "gui/side_panel.h"
#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

namespace
{

enum class StatusField : std::uint8_t
{
    Pointer = 0,
    Center,
    Zoom,
    Iterations,
    Render,
    Count,
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

constexpr double kMenuZoomFactor = 2.0;

}  // namespace

MainFrame::MainFrame(RenderSettings initial)
  : wxFrame(nullptr, wxID_ANY, "Mandelbrotter", wxDefaultPosition, wxDefaultSize),
    m_settings(std::move(initial)),
    m_canvas(new FractalCanvas(this, m_settings)),
    m_panel(new SidePanel(this))
{
    SetClientSize(FromDIP(wxSize(1280, 800)));

    auto* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(m_canvas, wxSizerFlags(1).Expand());
    sizer->Add(m_panel, wxSizerFlags().Expand());
    SetSizer(sizer);

    CreateStatusBar(field(StatusField::Count));
    constexpr std::array<int, field(StatusField::Count)> kWidths{-3, -3, -1, -1, -2};
    GetStatusBar()->SetStatusWidths(field(StatusField::Count), kWidths.data());

    buildMenus();
    wireCanvas();
    wirePanel();

    if (const std::string error = m_bookmarks.load(); !error.empty())
    {
        reportError("Bookmarks", "Could not read " + m_bookmarks.path().string() + ":\n" + error);
    }
    refreshBookmarks();
    applySettings(m_settings);
    m_canvas->SetFocus();
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
    help->Append(wxID_ABOUT, "&About Mandelbrotter");

    auto* bar = new wxMenuBar();
    bar->Append(file, "&File");
    bar->Append(view, "&View");
    bar->Append(bookmarks, "&Bookmarks");
    bar->Append(help, "&Help");
    SetMenuBar(bar);

    Bind(wxEVT_MENU, [this](wxCommandEvent&) { saveImage(); }, kMenuSaveImage);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { copyImage(); }, kMenuCopyImage);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { exportView(); }, kMenuExportView);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { importView(); }, kMenuImportView);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(true); }, wxID_EXIT);
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent& event) {
            GetSizer()->Show(m_panel, event.IsChecked());
            Layout();
        },
        kMenuShowPanel);
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent& event) {
            m_canvas->setShowOrbit(event.IsChecked());
            m_panel->setShowOrbit(event.IsChecked());
        },
        kMenuShowOrbit);
    Bind(
        wxEVT_MENU, [this](wxCommandEvent&) { m_canvas->zoomAtCenter(kMenuZoomFactor); },
        kMenuZoomIn);
    Bind(
        wxEVT_MENU, [this](wxCommandEvent&) { m_canvas->zoomAtCenter(1.0 / kMenuZoomFactor); },
        kMenuZoomOut);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { m_canvas->resetView(); }, kMenuResetView);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { addBookmark(); }, kMenuAddBookmark);
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&) {
            wxAboutDialogInfo info;
            info.SetName("Mandelbrotter");
            info.SetDescription(
                "Interactive Mandelbrot-family fractal explorer.\n\n"
                "Wheel: zoom at cursor.  Drag: pan.  Right-drag or Shift-drag: zoom to rectangle.\n"
                "Right-click: zoom out.  Arrows, +/-, Home: keyboard navigation.");
            wxAboutBox(info, this);
        },
        wxID_ABOUT);
}

// ---------------------------------------------------------------------------------------------------------------
// Wiring

void MainFrame::wireCanvas()
{
    m_canvas->onViewChanged = [this](const ViewSpec& view) {
        m_settings.view = view;
        m_panel->setSettings(m_settings);
        updateStatusBar();
    };
    m_canvas->onPointerMoved = [this](std::optional<Complex> pointer) {
        showPointer(pointer);
        m_panel->setPreviewSeed(pointer);
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
    m_panel->onOrbitToggled    = [this](bool enabled) {
        m_canvas->setShowOrbit(enabled);
        m_showOrbitItem->Check(enabled);
    };
    m_panel->onBookmarkAdd    = [this] { addBookmark(); };
    m_panel->onBookmarkLoad   = [this](std::size_t index) { loadBookmark(index); };
    m_panel->onBookmarkDelete = [this](std::size_t index) { deleteBookmark(index); };
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

void MainFrame::updateStatusBar()
{
    SetStatusText(toWx("Centre " + formatComplex(m_settings.view.center)),
                  field(StatusField::Center));
    SetStatusText(toWx("Zoom " + formatZoom(m_settings.view.zoom)), field(StatusField::Zoom));
    SetStatusText(toWx(std::format("{} iterations", effectiveIterations(m_settings))),
                  field(StatusField::Iterations));
    m_panel->setEffectiveIterations(effectiveIterations(m_settings));
}

void MainFrame::showPointer(std::optional<Complex> pointer)
{
    SetStatusText(pointer ? toWx(formatComplex(*pointer)) : wxString(),
                  field(StatusField::Pointer));
}

void MainFrame::showRenderStatus(const FractalCanvas::RenderStatus& status)
{
    if (status.rendering)
    {
        SetStatusText("Rendering...", field(StatusField::Render));
    }
    else
    {
        SetStatusText(toWx(std::format("Rendered in {} ms", status.elapsed.count())),
                      field(StatusField::Render));
    }
}

// ---------------------------------------------------------------------------------------------------------------
// File actions

void MainFrame::saveImage()
{
    ExportDialog dialog(this, m_canvas->currentImage().size());
    if (dialog.ShowModal() != wxID_OK)
    {
        return;
    }
    wxFileDialog chooser(this, "Save image as PNG", "", "mandelbrotter.png",
                         "PNG images (*.png)|*.png", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (chooser.ShowModal() != wxID_OK)
    {
        return;
    }
    const std::filesystem::path path(fromWx(chooser.GetPath()));
    if (exportPngWithProgress(this, m_settings, dialog.options(), path))
    {
        SetStatusText(toWx("Saved " + path.filename().string()), field(StatusField::Render));
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
    SetStatusText("Image copied to clipboard", field(StatusField::Render));
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
    if (const std::string error = m_bookmarks.save(); !error.empty())
    {
        reportError("Bookmarks", "Could not write " + m_bookmarks.path().string() + ":\n" + error);
    }
    refreshBookmarks();
}

void MainFrame::loadBookmark(std::size_t index)
{
    const auto& list = m_bookmarks.bookmarks();
    if (index < list.size())
    {
        applySettings(list[index].settings);
    }
}

void MainFrame::deleteBookmark(std::size_t index)
{
    m_bookmarks.remove(index);
    if (const std::string error = m_bookmarks.save(); !error.empty())
    {
        reportError("Bookmarks", "Could not write " + m_bookmarks.path().string() + ":\n" + error);
    }
    refreshBookmarks();
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
