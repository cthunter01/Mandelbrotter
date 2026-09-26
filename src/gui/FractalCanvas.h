#pragma once

#include <cstdint>

#include <wx/bitmap.h>
#include <wx/timer.h>
#include <wx/window.h>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/app/CanvasController.h"

namespace mandelbrotter::gui
{

/// The fractal view on screen: paints what its app::CanvasController holds and translates wx's
/// mouse, keyboard and size events into the controller's calls. The controller does the rest
/// (rendering, navigation, the orbit overlay); the application talks to it through controller().
class FractalCanvas : public wxWindow
{
public:
    FractalCanvas(wxWindow* parent, RenderSettings initial);
    ~FractalCanvas() override;
    FractalCanvas(const FractalCanvas&)            = delete;
    FractalCanvas& operator=(const FractalCanvas&) = delete;
    FractalCanvas(FractalCanvas&&)                 = delete;
    FractalCanvas& operator=(FractalCanvas&&)      = delete;

    [[nodiscard]] app::CanvasController&       controller() noexcept { return m_controller; }
    [[nodiscard]] const app::CanvasController& controller() const noexcept { return m_controller; }

private:
    void onPaint(wxPaintEvent& event);
    void onSize(wxSizeEvent& event);
    void onDpiChanged(wxDPIChangedEvent& event);
    void onResizeTimer(wxTimerEvent& event);
    void onMouseWheel(wxMouseEvent& event);
    void onLeftDown(wxMouseEvent& event);
    void onRightDown(wxMouseEvent& event);
    void onMotion(wxMouseEvent& event);
    void onMouseUp(wxMouseEvent& event);
    void onLeave(wxMouseEvent& event);
    void onCaptureLost(wxMouseCaptureLostEvent& event);
    void onKeyDown(wxKeyEvent& event);

    /// Tells the controller the client size and content scale.
    void updateSize();
    void drawOverlays(wxDC& dc);

    app::CanvasController m_controller;
    wxTimer               m_resizeTimer;
    wxBitmap              m_bitmap;
    std::uint64_t         m_bitmapVersion{0};  ///< the controller's imageVersion() m_bitmap shows
};

}  // namespace mandelbrotter::gui
