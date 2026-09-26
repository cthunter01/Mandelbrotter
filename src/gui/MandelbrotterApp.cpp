#include "gui/MandelbrotterApp.h"

#include <filesystem>
#include <utility>

#include <wx/filesys.h>
#include <wx/fs_arc.h>
#include <wx/fs_mem.h>
#include <wx/image.h>
#include <wx/stdpaths.h>

#include "Mandelbrotter/app/startup.h"
#include "gui/MainFrame.h"
#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

namespace
{

app::StartupOptions& startupOptionsStorage()
{
    static app::StartupOptions s_options;
    return s_options;
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

void setStartupOptions(app::StartupOptions options)
{
    startupOptionsStorage() = std::move(options);
}

const app::StartupOptions& startupOptions()
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

    const app::StartupOptions& options = startupOptions();
    std::filesystem::path      bookmarks =
        app::bookmarksPathFor(options, fromWx(wxStandardPaths::Get().GetUserDataDir()),
                              std::filesystem::temp_directory_path());
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
