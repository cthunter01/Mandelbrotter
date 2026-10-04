#include "gui/MainFrame.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
#include "Mandelbrotter/app/commands.h"
#include "Mandelbrotter/app/help_routing.h"
#include "Mandelbrotter/app/ui_text.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/exporter.h"
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

constexpr int field(app::StatusField f)
{
    return static_cast<int>(f);
}

/// A file dialog's filter in wx's "description (pattern)|pattern" form.
wxString filterText(const app::FileFilter& filter)
{
    return toWx(std::format("{} ({})|{}", filter.description, filter.pattern, filter.pattern));
}

}  // namespace

MainFrame::MainFrame(RenderSettings initial, std::filesystem::path bookmarksPath)
  : wxFrame(nullptr, wxID_ANY, toWx(app::kWindowTitle), wxDefaultPosition, wxDefaultSize),
    m_demoTimer(this),
    m_canvas(new FractalCanvas(this, initial)),
    m_panel(new SidePanel(this)),
    m_app(std::move(initial), std::move(bookmarksPath), m_canvas->controller(), makeShell()),
    m_tourView(*this)
{
    applyAppIcon(*this);
    SetClientSize(FromDIP(wxSize(app::kWindowSize.width, app::kWindowSize.height)));

    auto* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(m_canvas, wxSizerFlags(1).Expand());
    sizer->Add(m_panel, wxSizerFlags().Expand());
    SetSizer(sizer);

    CreateStatusBar(app::kStatusFieldCount);
    std::array<int, app::kStatusFieldCount> widths{};
    std::ranges::transform(app::kStatusStretch, widths.begin(),
                           [](int stretch) { return -stretch; });  // negative: proportional
    GetStatusBar()->SetStatusWidths(app::kStatusFieldCount, widths.data());

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
        .now = {},
        .tour =
            {
                .showCard = [this](const app::TourStep& step, std::size_t index,
                                   std::size_t count) { m_tourView.showCard(step, index, count); },
                .hideCard = [this] { m_tourView.hideCard(); },
                .highlight =
                    [this](std::optional<app::TourTarget> target) { m_tourView.highlight(target); },
            },
    };
}

// ---------------------------------------------------------------------------------------------------------------
// Menus

void MainFrame::buildMenus()
{
    auto* bar    = new wxMenuBar();
    int   nextId = wxID_HIGHEST + 1;
    for (const app::Menu& menu : app::menuBar())
    {
        auto* items = new wxMenu();
        appendMenuEntries(*items, menu.entries, nextId);
        bar->Append(items, toWx(menu.title));
    }
    SetMenuBar(bar);
}

void MainFrame::appendMenuEntries(wxMenu& menu, const std::vector<app::MenuEntry>& entries,
                                  int& nextId)
{
    using Kind = app::MenuEntry::Kind;
    for (const app::MenuEntry& entry : entries)
    {
        if (entry.kind == Kind::SEPARATOR)
        {
            menu.AppendSeparator();
            continue;
        }
        if (entry.kind == Kind::SUBMENU)
        {
            auto* submenu = new wxMenu();
            appendMenuEntries(*submenu, entry.children, nextId);
            menu.AppendSubMenu(submenu, toWx(entry.label), toWx(entry.statusTip));
            continue;
        }
        // Stock IDs let wx place the item where the platform wants it (macOS: the application
        // menu).
        int id = 0;
        switch (entry.role)
        {
            case app::MenuRole::QUIT:
                id = wxID_EXIT;
                break;
            case app::MenuRole::ABOUT:
                id = wxID_ABOUT;
                break;
            case app::MenuRole::CONTENTS:
                id = wxID_HELP_CONTENTS;
                break;
            case app::MenuRole::NONE:
                id = nextId++;
                break;
        }
        const std::string label =
            entry.shortcut.empty() ? entry.label : entry.label + "\t" + entry.shortcut;
        wxMenuItem* item = entry.kind == Kind::CHECK
                               ? menu.AppendCheckItem(id, toWx(label), toWx(entry.statusTip))
                               : menu.Append(id, toWx(label), toWx(entry.statusTip));
        if (entry.kind == Kind::CHECK)
        {
            item->Check(entry.checked);
        }
        if (entry.command == app::Command::SHOW_PANEL)
        {
            m_showPanelItem = item;
        }
        else if (entry.command == app::Command::SHOW_ORBIT)
        {
            m_showOrbitItem = item;
        }
        Bind(
            wxEVT_MENU,
            [this, command = entry.command, flight = entry.flight](wxCommandEvent& event) {
                runCommand(command, flight, event.IsChecked());
            },
            id);
        if (entry.enable != app::EnableRule::ALWAYS)
        {
            Bind(
                wxEVT_UPDATE_UI,
                [this, command = entry.command](wxUpdateUIEvent& event) {
                    event.Enable(m_app.commandEnabled(command));
                },
                id);
        }
    }
}

void MainFrame::runCommand(app::Command command, std::size_t flight, bool checked)
{
    if (m_app.runCommand(command, flight))
    {
        return;
    }
    switch (command)
    {
        case app::Command::SAVE_IMAGE:
            showExportDialog();
            break;
        case app::Command::COPY_IMAGE:
            copyImage();
            break;
        case app::Command::EXPORT_VIEW:
            exportView();
            break;
        case app::Command::IMPORT_VIEW:
            importView();
            break;
        case app::Command::QUIT:
            Close(true);
            break;
        case app::Command::SHOW_PANEL:
            setSidePanelShown(checked);
            break;
        case app::Command::SHOW_ORBIT:
            m_app.setShowOrbit(checked);
            break;
        case app::Command::ADD_BOOKMARK:
            onAddBookmark();
            break;
        case app::Command::CONTENTS:
            m_help.showContents();
            break;
        case app::Command::CONTEXT_HELP:
            showContextHelp();
            break;
        case app::Command::REFERENCE:
            m_help.showPage(app::kReferencePage);
            break;
        case app::Command::ABOUT:
            showAbout();
            break;
        case app::Command::ZOOM_IN:
        case app::Command::ZOOM_OUT:
        case app::Command::RESET_VIEW:
        case app::Command::FLIGHT:
        case app::Command::STOP_DEMO:
        case app::Command::TOUR:
        case app::Command::BACK_TO_SNAPSHOT:
            break;  // run by the app layer above
    }
}

void MainFrame::showAbout()
{
    wxAboutDialogInfo info;
    info.SetName(toWx(app::kAboutName));
    info.SetVersion(MANDELBROTTER_VERSION);
    info.SetDescription(toWx(app::kAboutDescription));
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
    m_help.onError  = [this](const std::string& message) {
        reportError(std::string(app::kHelpErrorTitle), message);
    };
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
    m_exportDialog->onHelp = [this] { m_help.showPage(app::kExportingPage); };
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
    wxFileDialog chooser(this, toWx(app::kSaveImageTitle), "", toWx(app::kSaveImageDefaultName),
                         filterText(app::kPngFilter), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
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
        reportError(std::string(app::kCopyImageTitle), std::string(app::kClipboardBusy));
        return;
    }
    // The clipboard takes ownership of the data object.
    wxTheClipboard->SetData(new wxBitmapDataObject(wxBitmap(toWxImage(image))));
    wxTheClipboard->Flush();  // keep the image available after this window closes
    m_app.imageCopied();
}

void MainFrame::exportView()
{
    wxFileDialog chooser(this, toWx(app::kExportViewTitle), "", toWx(app::kExportViewDefaultName),
                         filterText(app::kViewFilter), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (chooser.ShowModal() != wxID_OK)
    {
        return;
    }
    m_app.exportView(std::filesystem::path(fromWx(chooser.GetPath())));
}

void MainFrame::importView()
{
    // "All files" without its pattern in the description, as wx dialogs usually show it.
    const wxString filters =
        filterText(app::kViewFilter) +
        toWx(std::format("|{}|{}", app::kAllFilesFilter.description, app::kAllFilesFilter.pattern));
    wxFileDialog chooser(this, toWx(app::kImportViewTitle), "", "", filters,
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
    wxTextEntryDialog dialog(this, toWx(app::kAddBookmarkPrompt), toWx(app::kAddBookmarkTitle),
                             toWx(suggested));
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
