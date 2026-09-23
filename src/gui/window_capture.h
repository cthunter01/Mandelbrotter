#pragma once

#include <string>

#include <wx/gdicmn.h>
#include <wx/image.h>
#include <wx/window.h>

namespace mandelbrotter::gui
{

struct CaptureResult
{
    wxImage     image;         ///< invalid when the capture failed
    wxPoint     screenOrigin;  ///< where the image's top-left corner sits in screen coordinates
    std::string error;         ///< why, when the image is invalid
};

/// A picture of `window` as it is on screen (top-level windows without their decorations where
/// the platform allows). The developer screenshot mode uses it; the window must be mapped and
/// unobscured. Each platform has its own way to read pixels back, see window_capture.cpp.
[[nodiscard]] CaptureResult captureWindow(wxWindow& window);

}  // namespace mandelbrotter::gui
