#include "gui/MandelbrotterApp.h"

#include <wx/stdpaths.h>

#include "gui/MainFrame.h"

namespace mandelbrotter::gui
{

namespace
{

RenderSettings& startupSettingsStorage()
{
    static RenderSettings s_settings;
    return s_settings;
}

}  // namespace

void setStartupSettings(const RenderSettings& settings)
{
    startupSettingsStorage() = settings;
}

const RenderSettings& startupSettings()
{
    return startupSettingsStorage();
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

    auto* frame = new MainFrame(startupSettings());
    frame->Show(true);
    return true;
}

}  // namespace mandelbrotter::gui
