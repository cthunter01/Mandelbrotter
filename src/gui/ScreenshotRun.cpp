#include "gui/ScreenshotRun.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <wx/app.h>
#include <wx/dialog.h>
#include <wx/html/helpfrm.h>
#include <wx/sizer.h>
#include <wx/statusbr.h>
#include <wx/textdlg.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/flights.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "gui/FractalCanvas.h"
#include "gui/GuidedTour.h"
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

constexpr int     kSettleMs     = 500;
constexpr int     kWatchdogMs   = 120'000;
constexpr int     kMaxWidth     = 800;   ///< saved pictures are scaled down to this
constexpr int     kWindowWidth  = 1000;  ///< DIP; the whole window fits a help page after scaling
constexpr int     kWindowHeight = 640;
constexpr int     kCropMargin   = 4;
constexpr Complex kSeahorseValley{-0.7436, 0.1318};
constexpr Complex kOrbitPoint{0.285, 0.01};
constexpr Complex kJuliaSeed{-0.8, 0.156};
constexpr double  kDeepZoom = 1e12;

RenderSettings at(RenderSettings settings, Complex center, double zoom)
{
    settings.view.zoom   = clampZoom(zoom);
    settings.view.center = BigComplex::fromComplex(center, fractionBitsFor(settings.view.zoom));
    return settings;
}

RenderSettings mandelbrotDefault()
{
    RenderSettings settings;
    settings.view = defaultView(settings.fractal);
    return settings;
}

RenderSettings seahorse(const char* palette)
{
    RenderSettings settings   = at(mandelbrotDefault(), kSeahorseValley, 5000.0);
    settings.coloring.palette = palette;
    return settings;
}

/// The seahorse dive's destination (a point with structure at every depth) at `zoom`.
RenderSettings deepSeahorse(double zoom)
{
    RenderSettings settings = findFlight("seahorse-dive")->keyframes.back().settings;
    settings.view.zoom      = zoom;
    settings.view.center    = settings.view.center.withFractionBits(fractionBitsFor(zoom));
    return settings;
}

/// `rect` (client coordinates of `window`) in screen coordinates, with a small margin.
wxRect screenRectOf(wxWindow& window, const wxRect& rect)
{
    wxRect screen(window.ClientToScreen(rect.GetPosition()), rect.GetSize());
    return screen.Inflate(kCropMargin);
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
  : m_frame(frame), m_dir(std::move(dir)), m_settle(this), m_watchdog(this)
{
    Bind(wxEVT_TIMER, &ScreenshotRun::onSettled, this, m_settle.GetId());
    Bind(wxEVT_TIMER, &ScreenshotRun::onWatchdog, this, m_watchdog.GetId());
}

ScreenshotRun::~ScreenshotRun() = default;

std::vector<ScreenshotRun::Shot> ScreenshotRun::buildShots()
{
    MainFrame& frame       = m_frame;
    const auto whole       = [&frame]() -> wxWindow* { return &frame; };
    const auto noCrop      = []() -> std::optional<wxRect> { return std::nullopt; };
    const auto nothing     = [] { };
    const auto sectionCrop = [&frame](SidePanel::Section section) {
        return [&frame, section]() -> std::optional<wxRect> {
            return screenRectOf(frame.panel(), frame.panel().sectionRect(section));
        };
    };
    const auto showSection = [&frame](SidePanel::Section section) {
        return [&frame, section] { frame.panel().scrollToSection(section); };
    };
    const auto canvasCrop = [&frame]() -> std::optional<wxRect> {
        return screenRectOf(frame.canvas());
    };

    return {
        {.file         = "ui-main-window.png",
         .rendersFirst = true,
         .prepare      = [&frame] { frame.applySettings(mandelbrotDefault()); },
         .target       = whole,
         .crop         = noCrop,
         .cleanup      = nothing},
        {.file         = "ui-side-panel.png",
         .rendersFirst = true,
         .prepare =
             [&frame] {
                 // Tall enough for the whole panel (normally it scrolls).
                 const int panelHeight =
                     frame.panel().GetSizer()->GetMinSize().y + frame.FromDIP(16);
                 frame.SetClientSize(frame.FromDIP(kWindowWidth),
                                     std::max(frame.FromDIP(kWindowHeight), panelHeight));
                 frame.panel().scrollToSection(SidePanel::Section::FRACTAL);
             },
         .target = whole,
         .crop   = [&frame]() -> std::optional<wxRect> { return screenRectOf(frame.panel()); },
         .cleanup =
             [&frame] { frame.SetClientSize(frame.FromDIP(wxSize(kWindowWidth, kWindowHeight))); }},
        {.file         = "ui-fractal-section.png",
         .rendersFirst = true,
         .prepare =
             [&frame] {
                 RenderSettings settings;
                 settings.fractal.julia = true;
                 settings.fractal.seed  = kJuliaSeed;
                 settings.view          = defaultView(settings.fractal);
                 frame.applySettings(settings);
                 frame.panel().setPreviewSeed(kJuliaSeed);
             },
         .target  = whole,
         .crop    = sectionCrop(SidePanel::Section::FRACTAL),
         .cleanup = nothing},
        {.file         = "ui-iterations-section.png",
         .rendersFirst = true,
         .prepare      = [&frame] { frame.applySettings(seahorse("classic")); },
         .target       = whole,
         .crop         = sectionCrop(SidePanel::Section::ITERATIONS),
         .cleanup      = nothing},
        {.file         = "ui-colouring-section.png",
         .rendersFirst = false,
         .prepare =
             [&frame] {
                 RenderSettings settings   = seahorse("fire");
                 settings.coloring.density = 32.0;
                 settings.coloring.offset  = 0.25;
                 frame.applySettings(settings);
             },
         .target  = whole,
         .crop    = sectionCrop(SidePanel::Section::COLOURING),
         .cleanup = nothing},
        {.file         = "ui-overlay-section.png",
         .rendersFirst = false,
         .prepare =
             [&frame, showSection] {
                 frame.setShowOrbit(true);
                 showSection(SidePanel::Section::OVERLAY)();
             },
         .target  = whole,
         .crop    = sectionCrop(SidePanel::Section::OVERLAY),
         .cleanup = nothing},
        {.file         = "ui-bookmarks-section.png",
         .rendersFirst = false,
         .prepare      = showSection(SidePanel::Section::BOOKMARKS),
         .target       = whole,
         .crop         = sectionCrop(SidePanel::Section::BOOKMARKS),
         .cleanup      = nothing},
        {.file         = "ui-orbit-overlay.png",
         .rendersFirst = true,
         .prepare =
             [&frame] {
                 frame.applySettings(mandelbrotDefault());
                 frame.setShowOrbit(true);
                 frame.canvas().showOrbitAt(kOrbitPoint);
             },
         .target  = whole,
         .crop    = canvasCrop,
         .cleanup = [&frame] { frame.canvas().clearPinnedOrbit(); }},
        {.file         = "ui-canvas-seahorse.png",
         .rendersFirst = true,
         .prepare =
             [&frame] {
                 frame.setShowOrbit(false);
                 frame.applySettings(seahorse("electric"));
             },
         .target  = whole,
         .crop    = canvasCrop,
         .cleanup = nothing},
        {.file         = "ui-status-bar.png",
         .rendersFirst = true,
         .prepare =
             [&frame] {
                 frame.panel().scrollToSection(SidePanel::Section::FRACTAL);
                 frame.applySettings(deepSeahorse(kDeepZoom));
             },
         .target = whole,
         .crop   = [&frame]() -> std::optional<wxRect> {
             return screenRectOf(*frame.GetStatusBar());
         },
         .cleanup = nothing},
        {.file         = "ui-deep-zoom.png",
         .rendersFirst = false,
         .prepare      = nothing,
         .target       = whole,
         .crop         = noCrop,
         .cleanup      = nothing},
        {.file         = "ui-export-dialog.png",
         .rendersFirst = false,
         .prepare      = [&frame] { frame.showExportDialog(); },
         .target       = [&frame]() -> wxWindow* { return frame.exportDialog(); },
         .crop         = noCrop,
         .cleanup      = [&frame] { frame.closeExportDialog(); }},
        {.file         = "ui-add-bookmark-dialog.png",
         .rendersFirst = false,
         .prepare =
             [this] {
                 m_dialog = new wxTextEntryDialog(&m_frame, "Name for this view:", "Add bookmark",
                                                  "Mandelbrot at 1x");
                 m_dialog->Show();
             },
         .target = [this]() -> wxWindow* { return m_dialog; },
         .crop   = noCrop,
         .cleanup =
             [this] {
                 if (m_dialog != nullptr)
                 {
                     m_dialog->Destroy();
                     m_dialog = nullptr;
                 }
             }},
        {.file         = "ui-help-window.png",
         .rendersFirst = false,
         .prepare      = [&frame] { frame.help().showContents(); },
         .target       = [&frame]() -> wxWindow* { return frame.help().GetFrame(); },
         .crop         = noCrop,
         .cleanup =
             [&frame] {
                 if (wxFrame* helpFrame = frame.help().GetFrame(); helpFrame != nullptr)
                 {
                     helpFrame->Close(true);
                 }
             }},
        {.file         = "ui-tour-card.png",
         .rendersFirst = true,
         .prepare =
             [&frame] {
                 frame.startTour();
                 frame.tour().showStep(2);  // the Families step: card beside a highlighted section
             },
         .target  = whole,
         .crop    = noCrop,
         .cleanup = [&frame] { frame.stopDemos(); }},
    };
}

void ScreenshotRun::start()
{
    std::filesystem::create_directories(m_dir);
    m_shots = buildShots();
    m_frame.SetClientSize(m_frame.FromDIP(wxSize(kWindowWidth, kWindowHeight)));
    m_frame.onRenderFinished = [this] { onRenderFinished(); };
    // Let the window map and lay itself out before the first shot.
    CallAfter([this] { runCurrent(); });
}

void ScreenshotRun::runCurrent()
{
    if (m_done)
    {
        return;
    }
    if (m_index >= m_shots.size())
    {
        finish(0);
        return;
    }
    const Shot& shot = m_shots[m_index];
    m_watchdog.StartOnce(kWatchdogMs);  // per shot
    shot.prepare();
    m_waitingForRender = shot.rendersFirst && m_frame.canvas().rendering();
    if (!m_waitingForRender)
    {
        settle();
    }
}

void ScreenshotRun::onRenderFinished()
{
    if (m_waitingForRender)
    {
        m_waitingForRender = false;
        settle();
    }
}

void ScreenshotRun::settle()
{
    // A full repaint first: after a long render GTK on X11 can leave the panel's static box frames
    // undrawn until the next one. The pause then lets the toolkit paint.
    m_frame.Refresh();
    m_settle.StartOnce(kSettleMs);
}

void ScreenshotRun::onSettled(wxTimerEvent& /*event*/)
{
    capture();
}

void ScreenshotRun::onWatchdog(wxTimerEvent& /*event*/)
{
    fail("timed out");
}

void ScreenshotRun::capture()
{
    if (m_done || m_index >= m_shots.size())
    {
        return;
    }
    const Shot& shot   = m_shots[m_index];
    wxWindow*   target = shot.target();
    if (target == nullptr)
    {
        fail(shot.file + ": the window to capture does not exist");
        return;
    }
    target->Raise();
    const CaptureResult captured = captureWindow(*target);
    if (!captured.image.IsOk())
    {
        fail(shot.file + ": " + captured.error);
        return;
    }
    wxImage image = captured.image;
    if (isAllBlack(image))
    {
        fail(shot.file + ": the capture is all black (on Wayland, rerun with GDK_BACKEND=x11)");
        return;
    }
    if (const std::optional<wxRect> crop = shot.crop())
    {
        wxRect rect = *crop;
        rect.Offset(-captured.screenOrigin.x, -captured.screenOrigin.y);
        rect.Intersect(wxRect(image.GetSize()));
        if (rect.IsEmpty())
        {
            fail(shot.file + ": the region to keep lies outside the captured window");
            return;
        }
        image = image.GetSubImage(rect);
    }
    const double scale = target->GetContentScaleFactor();
    if (scale > 1.0)
    {
        image.Rescale(static_cast<int>(image.GetWidth() / scale),
                      static_cast<int>(image.GetHeight() / scale), wxIMAGE_QUALITY_HIGH);
    }
    if (image.GetWidth() > kMaxWidth)
    {
        image.Rescale(kMaxWidth, std::max(1, image.GetHeight() * kMaxWidth / image.GetWidth()),
                      wxIMAGE_QUALITY_HIGH);
    }
    const std::filesystem::path path = m_dir / shot.file;
    if (!image.SaveFile(toWx(path.string()), wxBITMAP_TYPE_PNG))
    {
        fail(shot.file + ": could not write " + path.string());
        return;
    }
    std::println("  {} ({}x{})", shot.file, image.GetWidth(), image.GetHeight());
    shot.cleanup();
    ++m_index;
    CallAfter([this] { runCurrent(); });
}

void ScreenshotRun::fail(const std::string& why)
{
    std::println(stderr, "error: screenshots: {}", why);
    finish(1);
}

void ScreenshotRun::finish(int exitCode)
{
    if (m_done)
    {
        return;
    }
    m_done = true;
    m_settle.Stop();
    m_watchdog.Stop();
    m_frame.onRenderFinished = nullptr;
    if (m_dialog != nullptr)
    {
        m_dialog->Destroy();
        m_dialog = nullptr;
    }
    if (auto* app = dynamic_cast<MandelbrotterApp*>(wxTheApp); app != nullptr)
    {
        app->setExitCode(exitCode);
    }
    // The scratch bookmarks the app created for this run (MandelbrotterApp::OnInit).
    std::error_code ignored;
    std::filesystem::remove_all(m_frame.bookmarksPath().parent_path(), ignored);
    if (exitCode == 0)
    {
        std::println("Screenshots written to {}", m_dir.string());
    }
    m_frame.CallAfter([this] { m_frame.Close(true); });
}

}  // namespace mandelbrotter::gui
