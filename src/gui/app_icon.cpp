#include "gui/app_icon.h"

#include <wx/iconbndl.h>

#ifdef __WXGTK__
#include <string>

#include <wx/bitmap.h>
#include <wx/icon.h>
#include <wx/image.h>
#include <wx/mstream.h>
#elifdef __WXMSW__
#elifdef __WXOSX__
#else
#error "app_icon.cpp needs a branch for this platform"
#endif

namespace mandelbrotter::gui
{

#ifdef __WXGTK__

void applyAppIcon(wxTopLevelWindow& window)
{
    std::string png;
    for (const std::string_view chunk : appIconPngChunks())
    {
        png.append(chunk);
    }
    wxMemoryInputStream stream(png.data(), png.size());
    wxImage             image;
    if (!image.LoadFile(stream, wxBITMAP_TYPE_PNG))
    {
        return;
    }
    // Every size the desktop may ask for, scaled here with a good filter rather than by the window
    // manager.
    wxIconBundle icons;
    for (const int size : {16, 24, 32, 48, 64, 128, 256})
    {
        wxIcon icon;
        icon.CopyFromBitmap(wxBitmap(image.Scale(size, size, wxIMAGE_QUALITY_HIGH)));
        icons.AddIcon(icon);
    }
    window.SetIcons(icons);
}

#elifdef __WXMSW__

void applyAppIcon(wxTopLevelWindow& window)
{
    // Every size of the icon that src/Mandelbrotter.rc compiles into the executable.
    window.SetIcons(wxIconBundle("appicon", nullptr));
}

#elifdef __WXOSX__

void applyAppIcon(wxTopLevelWindow& /*window*/) { }

#endif

}  // namespace mandelbrotter::gui
