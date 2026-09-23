#include "gui/MandelbrotterApp.h"

#include <filesystem>
#include <utility>
#include <vector>

#include <wx/filesys.h>
#include <wx/fs_arc.h>
#include <wx/fs_mem.h>
#include <wx/image.h>
#include <wx/stdpaths.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "gui/MainFrame.h"
#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

namespace
{

StartupOptions& startupOptionsStorage()
{
    static StartupOptions s_options;
    return s_options;
}

std::filesystem::path userBookmarksPath()
{
    return std::filesystem::path(fromWx(wxStandardPaths::Get().GetUserDataDir())) /
           "bookmarks.json";
}

RenderSettings viewAt(FractalFamily family, Complex center, double zoom, const char* palette)
{
    RenderSettings settings;
    settings.fractal.family = family;
    settings.view.zoom      = clampZoom(zoom);
    settings.view.center    = BigComplex::fromComplex(center, fractionBitsFor(settings.view.zoom));
    settings.coloring.palette = palette;
    return settings;
}

/// The screenshot mode shows a few bookmarks in the side panel without touching the user's file:
/// a scratch bookmarks file under the temp directory (removed by ScreenshotRun).
std::filesystem::path scratchBookmarksPath()
{
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "Mandelbrotter-screenshots";
    std::filesystem::create_directories(dir);
    const std::filesystem::path path = dir / "bookmarks.json";
    saveBookmarks(
        path,
        std::vector<Bookmark>{
            {.name     = "Seahorse Valley",
             .settings = viewAt(FractalFamily::MANDELBROT, {-0.7436, 0.1318}, 5000.0, "electric")},
            {.name     = "Elephant Valley",
             .settings = viewAt(FractalFamily::MANDELBROT, {0.275, 0.006}, 60.0, "fire")},
            {.name     = "The little ship",
             .settings = viewAt(FractalFamily::BURNING_SHIP, {-1.75, -0.03}, 40.0, "fire")}});
    return path;
}

// The embedded help book is served as "memory:help.zip#zip:..." (HelpController). wxFileSystem
// takes ownership of the handlers and deletes them at exit.
// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
void registerFileSystemHandlers()
{
    wxFileSystem::AddHandler(new wxMemoryFSHandler);
    wxFileSystem::AddHandler(new wxArchiveFSHandler);
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

}  // namespace

void setStartupOptions(StartupOptions options)
{
    startupOptionsStorage() = std::move(options);
}

const StartupOptions& startupOptions()
{
    return startupOptionsStorage();
}

bool MandelbrotterApp::OnInit()
{
    if (!wxApp::OnInit())
    {
        return false;
    }
    SetAppName("Mandelbrotter");
    SetVendorName("Mandelbrotter");
    wxStandardPaths::Get().SetFileLayout(wxStandardPaths::FileLayout_XDG);
    wxInitAllImageHandlers();  // PNG: the help book's pictures and the screenshot mode
    registerFileSystemHandlers();

    const StartupOptions& options = startupOptions();
    std::filesystem::path bookmarks =
        options.screenshotsDir ? scratchBookmarksPath() : userBookmarksPath();
    auto* frame = new MainFrame(options.settings, std::move(bookmarks));
    frame->Show(true);
    if (options.screenshotsDir)
    {
        frame->startScreenshotRun(*options.screenshotsDir);
    }
    return true;
}

int MandelbrotterApp::OnRun()
{
    const int code = wxApp::OnRun();
    return code != 0 ? code : m_exitCode;
}

}  // namespace mandelbrotter::gui
