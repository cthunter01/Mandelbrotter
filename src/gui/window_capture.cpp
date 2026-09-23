// The one place with platform code: reading a window's pixels back. wx cannot do it on GTK3
// (wxGTKCairoDCImpl::DoGetAsBitmap is not implemented), so that branch talks to GDK directly.
#include "gui/window_capture.h"

#include <string>

#include <wx/bitmap.h>
#include <wx/gdicmn.h>
#include <wx/image.h>
#include <wx/window.h>

#ifdef __WXGTK__
#include <gtk/gtk.h>
#elifdef __WXMSW__
#include <wx/dcmemory.h>
#include <wx/dcscreen.h>
#elifdef __WXOSX__
#include <wx/dcclient.h>
#include <wx/dcscreen.h>
#else
#error "window_capture.cpp needs a branch for this platform"
#endif

namespace mandelbrotter::gui
{

namespace
{

CaptureResult failure(std::string why)
{
    return {.image = wxImage(), .screenOrigin = wxPoint(), .error = std::move(why)};
}

}  // namespace

#ifdef __WXGTK__

CaptureResult captureWindow(wxWindow& window)
{
    GtkWidget* widget = window.GetHandle();  // WXWidget is GtkWidget* on wxGTK
    if (widget == nullptr)
    {
        return failure("the window has no GTK widget");
    }
    // A top-level GtkWindow that draws its own title bar (client-side decorations) also draws
    // shadows around it; its child, the box with menu bar, client area and status bar, is then the
    // content. With server-side decorations the whole GdkWindow is the content.
    GtkWidget* content = widget;
    if (window.IsTopLevel() && GTK_IS_WINDOW(widget) &&
        gtk_window_get_titlebar(GTK_WINDOW(widget)) != nullptr && GTK_IS_BIN(widget))
    {
        if (GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget)); child != nullptr)
        {
            content = child;
        }
    }
    GdkWindow* gdkWindow = gtk_widget_get_window(content);
    if (gdkWindow == nullptr)
    {
        return failure("the window is not realized");
    }
    int x      = 0;
    int y      = 0;
    int width  = gdk_window_get_width(gdkWindow);
    int height = gdk_window_get_height(gdkWindow);
    if (content != widget && gtk_widget_get_has_window(content) == 0)
    {
        // A widget without its own GdkWindow draws into its parent's at its allocation.
        GtkAllocation allocation;
        gtk_widget_get_allocation(content, &allocation);
        x      = allocation.x;
        y      = allocation.y;
        width  = allocation.width;
        height = allocation.height;
    }

    GdkPixbuf* pixbuf = gdk_pixbuf_get_from_window(gdkWindow, x, y, width, height);
    if (pixbuf == nullptr)
    {
        return failure(
            "gdk_pixbuf_get_from_window returned nothing (on Wayland, rerun with GDK_BACKEND=x11)");
    }
    const wxBitmap bitmap(pixbuf);  // takes over the reference
    int            originX = 0;
    int            originY = 0;
    gdk_window_get_origin(gdkWindow, &originX, &originY);
    wxImage image = bitmap.ConvertToImage();
    if (image.HasAlpha())
    {
        image.ClearAlpha();  // the content area is opaque; keep the PNGs at 24 bits
    }
    return {.image = image, .screenOrigin = wxPoint(originX + x, originY + y), .error = {}};
}

#elifdef __WXMSW__

CaptureResult captureWindow(wxWindow& window)
{
    const wxRect rect = window.GetScreenRect();  // for a frame this includes its title bar
    wxBitmap     bitmap(rect.width, rect.height);
    {
        wxScreenDC screen;
        wxMemoryDC memory(bitmap);
        if (!memory.Blit(0, 0, rect.width, rect.height, &screen, rect.x, rect.y))
        {
            return failure("could not copy the screen");
        }
    }
    return {.image = bitmap.ConvertToImage(), .screenOrigin = rect.GetPosition(), .error = {}};
}

#elifdef __WXOSX__

CaptureResult captureWindow(wxWindow& window)
{
    wxClientDC clientDc(&window);
    if (wxBitmap bitmap = clientDc.GetAsBitmap(); bitmap.IsOk())
    {
        return {.image        = bitmap.ConvertToImage(),
                .screenOrigin = window.ClientToScreen(wxPoint(0, 0)),
                .error        = {}};
    }
    const wxRect rect = window.GetScreenRect();
    wxScreenDC   screen;
    wxBitmap     bitmap = screen.GetAsBitmap(&rect);
    if (!bitmap.IsOk())
    {
        return failure(
            "could not read the screen (System Settings > Privacy > Screen Recording must allow "
            "this program)");
    }
    return {.image = bitmap.ConvertToImage(), .screenOrigin = rect.GetPosition(), .error = {}};
}

#endif

}  // namespace mandelbrotter::gui
