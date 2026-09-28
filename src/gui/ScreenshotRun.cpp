#include "gui/ScreenshotRun.h"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <wx/app.h>
#include <wx/dialog.h>
#include <wx/html/helpfrm.h>
#include <wx/sizer.h>
#include <wx/statusbr.h>
#include <wx/textdlg.h>

#include "Mandelbrotter/app/ScreenshotScript.h"
#include "Mandelbrotter/app/ui_text.h"
#include "gui/FractalCanvas.h"
#include "gui/HelpController.h"
#include "gui/MainFrame.h"
#include "gui/MandelbrotterApp.h"
#include "gui/SidePanel.h"
#include "gui/window_capture.h"
#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

namespace
{

/// `rect` (client coordinates of `window`) in screen coordinates, with the margin.
wxRect screenRectOf(wxWindow& window, const wxRect& rect)
{
    wxRect screen(window.ClientToScreen(rect.GetPosition()), rect.GetSize());
    return screen.Inflate(app::kScreenshotMarginPx);
}

wxRect screenRectOf(wxWindow& window)
{
    return screenRectOf(window, wxRect(window.GetClientSize()));
}

/// What a compositor that refuses to hand pixels back produces.
bool isAllBlack(const wxImage& image)
{
    const std::span<const unsigned char> pixels(
        image.GetData(), static_cast<std::size_t>(image.GetWidth()) *
                             static_cast<std::size_t>(image.GetHeight()) * 3);
    return std::ranges::all_of(pixels, [](unsigned char byte) { return byte == 0; });
}

}  // namespace

ScreenshotRun::ScreenshotRun(MainFrame& frame, std::filesystem::path dir)
  : m_frame(frame),
    m_settle(this),
    m_watchdog(this),
    m_script(frame.app(), std::move(dir), makeHooks())
{
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { m_script.settled(); }, m_settle.GetId());
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { m_script.watchdogFired(); }, m_watchdog.GetId());
}

ScreenshotRun::~ScreenshotRun() = default;

void ScreenshotRun::start()
{
    m_script.start();
}

app::ScreenshotScript::Hooks ScreenshotRun::makeHooks()
{
    MainFrame& frame = m_frame;
    return {
        .setContentSize =
            [&frame](PixelSize size) {
                frame.SetClientSize(frame.FromDIP(wxSize(size.width, size.height)));
            },
        .growToWholePanel =
            [&frame] {
                const int panelHeight = frame.panel().GetSizer()->GetMinSize().y +
                                        frame.FromDIP(app::kScreenshotPanelSlack);
                frame.SetClientSize(
                    frame.FromDIP(app::kScreenshotContentSize.width),
                    std::max(frame.FromDIP(app::kScreenshotContentSize.height), panelHeight));
            },
        .scrollPanelTo =
            [&frame](app::PanelSection section) { frame.panel().scrollToSection(section); },
        .showBookmarkDialog =
            [this](std::string_view text) {
                closeBookmarkDialog();
                m_dialog = new wxTextEntryDialog(&m_frame, toWx(app::kAddBookmarkPrompt),
                                                 toWx(app::kAddBookmarkTitle), toWx(text));
                m_dialog->Show();
            },
        .closeBookmarkDialog = [this] { closeBookmarkDialog(); },
        .showHelpContents    = [&frame] { frame.help().showContents(); },
        .closeHelp =
            [&frame] {
                if (wxFrame* helpFrame = frame.help().GetFrame(); helpFrame != nullptr)
                {
                    helpFrame->Close(true);
                }
            },
        .refreshAll = [&frame] { frame.Refresh(); },
        .capture = [this](
                       app::ShotTarget target, app::ShotRegion region,
                       const std::filesystem::path& path) { return capture(target, region, path); },
        .startSettleTimer =
            [this](std::chrono::milliseconds delay) {
                m_settle.StartOnce(static_cast<int>(delay.count()));
            },
        .startWatchdog =
            [this](std::chrono::milliseconds delay) {
                m_watchdog.StartOnce(static_cast<int>(delay.count()));
            },
        .stopTimers =
            [this] {
                m_settle.Stop();
                m_watchdog.Stop();
            },
        .post = [this](const std::function<void()>& work) { CallAfter(work); },
        .finished =
            [&frame](int exitCode) {
                if (auto* app = dynamic_cast<MandelbrotterApp*>(wxTheApp); app != nullptr)
                {
                    app->setExitCode(exitCode);
                }
                frame.CallAfter([&frame] { frame.Close(true); });
            },
        .print      = [](std::string_view line) { std::println("{}", line); },
        .printError = [](std::string_view line) { std::println(stderr, "{}", line); },
    };
}

void ScreenshotRun::closeBookmarkDialog()
{
    if (m_dialog != nullptr)
    {
        m_dialog->Destroy();
        m_dialog = nullptr;
    }
}

wxWindow* ScreenshotRun::targetWindow(app::ShotTarget target)
{
    switch (target)
    {
        case app::ShotTarget::MAIN:
            return &m_frame;
        case app::ShotTarget::EXPORT_DIALOG:
            return m_frame.exportDialog();
        case app::ShotTarget::BOOKMARK_DIALOG:
            return m_dialog;
        case app::ShotTarget::HELP:
            return m_frame.help().GetFrame();
    }
    return nullptr;
}

std::optional<wxRect> ScreenshotRun::regionRect(app::ShotRegion region)
{
    switch (region.kind)
    {
        case app::ShotRegion::Kind::WHOLE:
            break;
        case app::ShotRegion::Kind::PANEL:
            return screenRectOf(m_frame.panel());
        case app::ShotRegion::Kind::SECTION:
            return screenRectOf(m_frame.panel(), m_frame.panel().sectionRect(region.section));
        case app::ShotRegion::Kind::CANVAS:
            return screenRectOf(m_frame.canvas());
        case app::ShotRegion::Kind::STATUS_BAR:
            return screenRectOf(*m_frame.GetStatusBar());
    }
    return std::nullopt;
}

std::expected<PixelSize, std::string> ScreenshotRun::capture(app::ShotTarget              target,
                                                             app::ShotRegion              region,
                                                             const std::filesystem::path& path)
{
    wxWindow* window = targetWindow(target);
    if (window == nullptr)
    {
        return std::unexpected("the window to capture does not exist");
    }
    window->Raise();
    const CaptureResult captured = captureWindow(*window);
    if (!captured.image.IsOk())
    {
        return std::unexpected(captured.error);
    }
    wxImage image = captured.image;
    if (isAllBlack(image))
    {
        return std::unexpected("the capture is all black (on Wayland, rerun with GDK_BACKEND=x11)");
    }
    if (const std::optional<wxRect> crop = regionRect(region))
    {
        wxRect rect = *crop;
        rect.Offset(-captured.screenOrigin.x, -captured.screenOrigin.y);
        rect.Intersect(wxRect(image.GetSize()));
        if (rect.IsEmpty())
        {
            return std::unexpected("the region to keep lies outside the captured window");
        }
        image = image.GetSubImage(rect);
    }
    const double scale = window->GetContentScaleFactor();
    if (scale > 1.0)
    {
        image.Rescale(static_cast<int>(image.GetWidth() / scale),
                      static_cast<int>(image.GetHeight() / scale), wxIMAGE_QUALITY_HIGH);
    }
    constexpr int kMaxWidth = app::kScreenshotMaxWidth;
    if (image.GetWidth() > kMaxWidth)
    {
        image.Rescale(kMaxWidth, std::max(1, image.GetHeight() * kMaxWidth / image.GetWidth()),
                      wxIMAGE_QUALITY_HIGH);
    }
    if (!image.SaveFile(toWx(path.string()), wxBITMAP_TYPE_PNG))
    {
        return std::unexpected("could not write " + path.string());
    }
    return PixelSize{image.GetWidth(), image.GetHeight()};
}

}  // namespace mandelbrotter::gui
