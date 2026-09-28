#include "gui/FractalCanvas.h"

#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/pen.h>
#include <wx/settings.h>

#include "Mandelbrotter/app/ui_text.h"
#include "Mandelbrotter/geometry.h"
#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

FractalCanvas::FractalCanvas(wxWindow* parent, RenderSettings initial)
  : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
             wxWANTS_CHARS | wxFULL_REPAINT_ON_RESIZE),
    m_controller(
        std::move(initial),
        {.post           = [this](const std::function<void()>& work) { CallAfter(work); },
         .requestRepaint = [this] { Refresh(false); },
         .setCursor =
             [this](app::CanvasCursor cursor) {
                 SetCursor(wxCursor(cursor == app::CanvasCursor::BULLSEYE ? wxCURSOR_BULLSEYE
                                                                          : wxCURSOR_CROSS));
             },
         .captureMouse =
             [this](bool on) {
                 if (on && !HasCapture())
                 {
                     CaptureMouse();
                 }
                 else if (!on && HasCapture())
                 {
                     ReleaseMouse();
                 }
             }}),
    m_resizeTimer(this)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(FromDIP(wxSize(app::kCanvasMinSize.width, app::kCanvasMinSize.height)));
    SetCursor(wxCursor(wxCURSOR_CROSS));
    updateSize();

    Bind(wxEVT_PAINT, &FractalCanvas::onPaint, this);
    Bind(wxEVT_SIZE, &FractalCanvas::onSize, this);
    Bind(wxEVT_DPI_CHANGED, &FractalCanvas::onDpiChanged, this);
    Bind(wxEVT_TIMER, &FractalCanvas::onResizeTimer, this, m_resizeTimer.GetId());
    Bind(wxEVT_MOUSEWHEEL, &FractalCanvas::onMouseWheel, this);
    Bind(wxEVT_LEFT_DOWN, &FractalCanvas::onLeftDown, this);
    Bind(wxEVT_RIGHT_DOWN, &FractalCanvas::onRightDown, this);
    Bind(wxEVT_MOTION, &FractalCanvas::onMotion, this);
    Bind(wxEVT_LEFT_UP, &FractalCanvas::onMouseUp, this);
    Bind(wxEVT_RIGHT_UP, &FractalCanvas::onMouseUp, this);
    Bind(wxEVT_LEAVE_WINDOW, &FractalCanvas::onLeave, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &FractalCanvas::onCaptureLost, this);
    Bind(wxEVT_KEY_DOWN, &FractalCanvas::onKeyDown, this);
}

FractalCanvas::~FractalCanvas()
{
    // Stop the workers before wxWindow tears down: their callbacks post events to this handler.
    m_controller.cancelRender();
    DeletePendingEvents();
}

void FractalCanvas::updateSize()
{
    m_controller.setSize(fromWx(GetClientSize()), GetContentScaleFactor());
}

// ---------------------------------------------------------------------------------------------------------------
// Painting

void FractalCanvas::onPaint(wxPaintEvent& /*event*/)
{
    wxAutoBufferedPaintDC dc(this);
    const RgbImage&       image = m_controller.image();
    if (m_bitmapVersion != m_controller.imageVersion() && !image.size().empty())
    {
        m_bitmap        = wxBitmap(toWxImage(image), -1, GetContentScaleFactor());
        m_bitmapVersion = m_controller.imageVersion();
    }
    const wxPoint offset = toWx(m_controller.panOffset());
    if (offset != wxPoint() || !m_bitmap.IsOk())
    {
        dc.SetBackground(*wxBLACK_BRUSH);
        dc.Clear();
    }
    if (m_bitmap.IsOk())
    {
        dc.DrawBitmap(m_bitmap, offset.x, offset.y, false);
    }
    drawOverlays(dc);
}

void FractalCanvas::drawOverlays(wxDC& dc)
{
    if (const std::optional<PixelRect> band = m_controller.rubberBand())
    {
        const wxRect rect = toWx(*band);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(*wxBLACK, FromDIP(app::kRubberBandWidth), wxPENSTYLE_SOLID));
        dc.DrawRectangle(rect);
        dc.SetPen(wxPen(*wxWHITE, FromDIP(app::kRubberBandWidth), wxPENSTYLE_SHORT_DASH));
        dc.DrawRectangle(rect);
    }
    const std::span<const PixelPoint> orbit = m_controller.orbit();
    if (m_controller.showOrbit() && orbit.size() > 1)
    {
        std::vector<wxPoint> points;
        points.reserve(orbit.size());
        for (const PixelPoint point : orbit)
        {
            points.push_back(toWx(point));
        }
        dc.SetPen(wxPen(toWx(app::kOrbitColor), FromDIP(app::kOrbitWidth), wxPENSTYLE_SOLID));
        dc.DrawLines(static_cast<int>(points.size()), points.data());
        dc.SetPen(
            wxPen(toWx(app::kOrbitStartColor), FromDIP(app::kOrbitStartWidth), wxPENSTYLE_SOLID));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawCircle(points.front(), FromDIP(app::kOrbitStartRadius));
    }
    if (m_controller.highlighted())
    {
        const int width = FromDIP(app::kHighlightRingWidth);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT), width));
        dc.DrawRectangle(wxRect(GetClientSize()).Deflate(width / 2));
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Size

void FractalCanvas::onSize(wxSizeEvent& event)
{
    updateSize();
    m_resizeTimer.StartOnce(static_cast<int>(app::kResizeDebounce.count()));
    event.Skip();
}

void FractalCanvas::onDpiChanged(wxDPIChangedEvent& event)
{
    updateSize();  // the content scale may change without a size event
    event.Skip();
}

void FractalCanvas::onResizeTimer(wxTimerEvent& /*event*/)
{
    m_controller.resizeSettled();
}

// ---------------------------------------------------------------------------------------------------------------
// Mouse

void FractalCanvas::onMouseWheel(wxMouseEvent& event)
{
    if (event.GetWheelAxis() != wxMOUSE_WHEEL_VERTICAL || event.GetWheelDelta() == 0)
    {
        return;
    }
    m_controller.wheel(fromWx(event.GetPosition()),
                       static_cast<double>(event.GetWheelRotation()) / event.GetWheelDelta());
}

void FractalCanvas::onLeftDown(wxMouseEvent& event)
{
    SetFocus();
    m_controller.buttonDown(app::MouseButton::LEFT, fromWx(event.GetPosition()), event.ShiftDown());
}

void FractalCanvas::onRightDown(wxMouseEvent& event)
{
    SetFocus();
    m_controller.buttonDown(app::MouseButton::RIGHT, fromWx(event.GetPosition()),
                            event.ShiftDown());
}

void FractalCanvas::onMotion(wxMouseEvent& event)
{
    m_controller.pointerMoved(fromWx(event.GetPosition()));
}

void FractalCanvas::onMouseUp(wxMouseEvent& event)
{
    m_controller.buttonUp(
        event.GetButton() == wxMOUSE_BTN_RIGHT ? app::MouseButton::RIGHT : app::MouseButton::LEFT,
        fromWx(event.GetPosition()));
}

void FractalCanvas::onLeave(wxMouseEvent& /*event*/)
{
    m_controller.pointerLeft();
}

void FractalCanvas::onCaptureLost(wxMouseCaptureLostEvent& /*event*/)
{
    m_controller.captureLost();
}

// ---------------------------------------------------------------------------------------------------------------
// Keyboard

void FractalCanvas::onKeyDown(wxKeyEvent& event)
{
    std::optional<app::CanvasKey> key;
    switch (event.GetKeyCode())
    {
        case WXK_LEFT:
            key = app::CanvasKey::LEFT;
            break;
        case WXK_RIGHT:
            key = app::CanvasKey::RIGHT;
            break;
        case WXK_UP:
            key = app::CanvasKey::UP;
            break;
        case WXK_DOWN:
            key = app::CanvasKey::DOWN;
            break;
        case '+':
        case '=':
        case WXK_NUMPAD_ADD:
        case WXK_PAGEUP:
            key = app::CanvasKey::ZOOM_IN;
            break;
        case '-':
        case WXK_NUMPAD_SUBTRACT:
        case WXK_PAGEDOWN:
            key = app::CanvasKey::ZOOM_OUT;
            break;
        case WXK_HOME:
            key = app::CanvasKey::HOME;
            break;
        case WXK_ESCAPE:
            key = app::CanvasKey::ESCAPE;
            break;
        default:
            break;
    }
    if (!key || !m_controller.key(*key))
    {
        event.Skip();
    }
}

}  // namespace mandelbrotter::gui
