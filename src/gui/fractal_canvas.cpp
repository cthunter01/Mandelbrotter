#include "gui/fractal_canvas.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/pen.h>

#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/kernel.h"
#include "Mandelbrotter/palette.h"
#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

namespace
{

constexpr int    kResizeDebounceMs       = 150;
constexpr int    kDragThresholdPx        = 3;
constexpr double kWheelZoomPerNotch      = 1.25;
constexpr double kKeyZoomFactor          = 2.0;
constexpr double kKeyPanFraction         = 0.1;
constexpr int    kMaxOrbitPoints         = 512;
constexpr int    kOverlayCoordinateLimit = 10000;

int clampCoordinate(int value)
{
    return std::clamp(value, -kOverlayCoordinateLimit, kOverlayCoordinateLimit);
}

}  // namespace

FractalCanvas::FractalCanvas(wxWindow* parent, RenderSettings initial)
  : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
             wxWANTS_CHARS | wxFULL_REPAINT_ON_RESIZE),
    m_settings(std::move(initial)),
    m_resizeTimer(this)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(FromDIP(wxSize(200, 150)));
    SetCursor(wxCursor(wxCURSOR_CROSS));

    Bind(wxEVT_PAINT, &FractalCanvas::onPaint, this);
    Bind(wxEVT_SIZE, &FractalCanvas::onSize, this);
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
    m_renderer.cancel();
    DeletePendingEvents();
}

// ---------------------------------------------------------------------------------------------------------------
// Settings

void FractalCanvas::setSettings(const RenderSettings& settings)
{
    const bool rerender =
        needsRerender(m_settings, settings) || m_iterations.size() != renderSize();
    m_settings = settings;
    if (rerender)
    {
        startRender();
    }
    else
    {
        recolor();
    }
}

void FractalCanvas::setPickSeedMode(bool enabled)
{
    m_pickSeedMode = enabled;
    SetCursor(wxCursor(enabled ? wxCURSOR_BULLSEYE : wxCURSOR_CROSS));
}

void FractalCanvas::setShowOrbit(bool enabled)
{
    m_showOrbit = enabled;
    if (!enabled)
    {
        m_orbit.clear();
    }
    Refresh(false);
}

void FractalCanvas::resetView()
{
    changeView(defaultView(m_settings.fractal));
}

void FractalCanvas::zoomAtCenter(double factor)
{
    const PixelSize size = renderSize();
    changeView(viewport().zoomedAt({size.width / 2, size.height / 2}, factor).view());
}

// ---------------------------------------------------------------------------------------------------------------
// Geometry helpers

double FractalCanvas::scale() const
{
    return GetContentScaleFactor();
}

PixelSize FractalCanvas::renderSize() const
{
    const wxSize client = GetClientSize();
    const double s      = scale();
    return {std::max(1, static_cast<int>(std::lround(client.x * s))),
            std::max(1, static_cast<int>(std::lround(client.y * s)))};
}

Viewport FractalCanvas::viewport() const
{
    return {m_settings.view, renderSize()};
}

PixelPoint FractalCanvas::toDevice(wxPoint p) const
{
    const double s = scale();
    return {static_cast<int>(std::floor(p.x * s)), static_cast<int>(std::floor(p.y * s))};
}

wxPoint FractalCanvas::toLogical(PixelPoint p) const
{
    const double s = scale();
    return {static_cast<int>(std::lround(p.x / s)), static_cast<int>(std::lround(p.y / s))};
}

Complex FractalCanvas::complexAt(wxPoint p) const
{
    return viewport().pixelCenter(toDevice(p));
}

// ---------------------------------------------------------------------------------------------------------------
// Rendering

void FractalCanvas::changeView(const ViewSpec& view)
{
    m_settings.view = view;
    startRender();
    if (onViewChanged)
    {
        onViewChanged(view);
    }
}

void FractalCanvas::startRender()
{
    const PixelSize size = renderSize();
    if (m_iterations.size() != size)
    {
        m_iterations = IterationBuffer(size.width, size.height);
    }
    if (m_rgb.size() != size)
    {
        m_rgb         = RgbImage(size.width, size.height);
        m_bitmapDirty = true;
    }

    RenderJob job;
    job.settings = m_settings;
    job.size     = size;
    m_generation = m_renderer.start(
        std::move(job),
        [this](const TileResult& tile) {
            auto copy = std::make_shared<TileResult>(tile);
            CallAfter([this, copy] { applyTile(*copy); });
        },
        [this](const RenderCompletion& completion) {
            CallAfter([this, completion] { finishRender(completion); });
        });
    reportStatus(true, {});
}

void FractalCanvas::applyTile(const TileResult& tile)
{
    if (tile.generation != m_generation)
    {
        return;
    }
    tile.copyInto(m_iterations);
    colorize(m_iterations, tile.rect, paletteOrDefault(m_settings.coloring.palette),
             m_settings.coloring, m_rgb);
    m_bitmapDirty = true;
    Refresh(false);
}

void FractalCanvas::finishRender(const RenderCompletion& completion)
{
    if (completion.generation != m_generation || completion.cancelled)
    {
        return;
    }
    reportStatus(false, completion.elapsed);
}

void FractalCanvas::recolor()
{
    if (m_iterations.size().empty())
    {
        return;
    }
    colorize(m_iterations, paletteOrDefault(m_settings.coloring.palette), m_settings.coloring,
             m_rgb);
    m_bitmapDirty = true;
    Refresh(false);
}

void FractalCanvas::reportStatus(bool rendering, std::chrono::milliseconds elapsed)
{
    if (onRenderStatus)
    {
        onRenderStatus({.rendering  = rendering,
                        .elapsed    = elapsed,
                        .iterations = effectiveIterations(m_settings)});
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Painting

void FractalCanvas::onPaint(wxPaintEvent& /*event*/)
{
    wxAutoBufferedPaintDC dc(this);
    if (m_bitmapDirty && !m_rgb.size().empty())
    {
        m_bitmap      = wxBitmap(toWxImage(m_rgb), -1, scale());
        m_bitmapDirty = false;
    }
    if (m_panOffset != wxPoint() || !m_bitmap.IsOk())
    {
        dc.SetBackground(*wxBLACK_BRUSH);
        dc.Clear();
    }
    if (m_bitmap.IsOk())
    {
        dc.DrawBitmap(m_bitmap, m_panOffset.x, m_panOffset.y, false);
    }
    drawOverlays(dc);
}

void FractalCanvas::drawOverlays(wxDC& dc)
{
    if (m_drag == Drag::RubberBand)
    {
        const wxRect rect(m_dragStart, m_dragCurrent);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(*wxBLACK, 1, wxPENSTYLE_SOLID));
        dc.DrawRectangle(rect);
        dc.SetPen(wxPen(*wxWHITE, 1, wxPENSTYLE_SHORT_DASH));
        dc.DrawRectangle(rect);
    }
    if (m_showOrbit && m_orbit.size() > 1)
    {
        dc.SetPen(wxPen(wxColour(255, 255, 255, 200), 1, wxPENSTYLE_SOLID));
        dc.DrawLines(static_cast<int>(m_orbit.size()), m_orbit.data());
        dc.SetPen(wxPen(wxColour(255, 80, 80), 2, wxPENSTYLE_SOLID));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawCircle(m_orbit.front(), FromDIP(4));
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Size

void FractalCanvas::onSize(wxSizeEvent& event)
{
    m_resizeTimer.StartOnce(kResizeDebounceMs);
    event.Skip();
}

void FractalCanvas::onResizeTimer(wxTimerEvent& /*event*/)
{
    if (m_iterations.size() != renderSize())
    {
        startRender();
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Mouse

void FractalCanvas::onMouseWheel(wxMouseEvent& event)
{
    if (event.GetWheelAxis() != wxMOUSE_WHEEL_VERTICAL || event.GetWheelDelta() == 0)
    {
        return;
    }
    const double notches = static_cast<double>(event.GetWheelRotation()) / event.GetWheelDelta();
    const double factor  = std::pow(kWheelZoomPerNotch, notches);
    changeView(viewport().zoomedAt(toDevice(event.GetPosition()), factor).view());
}

void FractalCanvas::onLeftDown(wxMouseEvent& event)
{
    SetFocus();
    if (m_pickSeedMode)
    {
        if (onSeedPicked)
        {
            onSeedPicked(complexAt(event.GetPosition()));
        }
        return;
    }
    beginDrag(event.ShiftDown() ? Drag::RubberBand : Drag::Pan, event.GetPosition());
}

void FractalCanvas::onRightDown(wxMouseEvent& event)
{
    SetFocus();
    beginDrag(Drag::RubberBand, event.GetPosition());
}

void FractalCanvas::beginDrag(Drag kind, wxPoint at)
{
    m_drag        = kind;
    m_dragStart   = at;
    m_dragCurrent = at;
    m_panOffset   = wxPoint();
    if (!HasCapture())
    {
        CaptureMouse();
    }
}

void FractalCanvas::onMotion(wxMouseEvent& event)
{
    const wxPoint at = event.GetPosition();
    if (m_drag == Drag::Pan)
    {
        m_panOffset = at - m_dragStart;
        Refresh(false);
    }
    else if (m_drag == Drag::RubberBand)
    {
        m_dragCurrent = at;
        Refresh(false);
    }
    if (onPointerMoved)
    {
        onPointerMoved(complexAt(at));
    }
    if (m_showOrbit && m_drag == Drag::None)
    {
        updateOrbit(at);
    }
}

void FractalCanvas::onMouseUp(wxMouseEvent& event)
{
    if (m_drag == Drag::None)
    {
        return;
    }
    endDrag(event.GetPosition(), event.GetButton() == wxMOUSE_BTN_RIGHT);
}

void FractalCanvas::endDrag(wxPoint at, bool rightButton)
{
    if (HasCapture())
    {
        ReleaseMouse();
    }
    const Drag kind     = m_drag;
    m_drag              = Drag::None;
    const wxPoint delta = at - m_dragStart;
    const bool moved = std::abs(delta.x) > kDragThresholdPx || std::abs(delta.y) > kDragThresholdPx;
    if (kind == Drag::Pan)
    {
        finishPan(at);
    }
    else if (moved)
    {
        finishRubberBand(at);
    }
    else if (rightButton)
    {
        changeView(viewport().zoomedAt(toDevice(at), 1.0 / kKeyZoomFactor).view());
    }
    else
    {
        Refresh(false);
    }
}

void FractalCanvas::finishPan(wxPoint at)
{
    const wxPoint delta = at - m_dragStart;
    m_panOffset         = wxPoint();
    if (delta == wxPoint())
    {
        Refresh(false);
        return;
    }
    const PixelPoint deviceDelta = toDevice(delta);
    // Keep the old picture, shifted, as the placeholder until the new render's first pass lands.
    RgbImage shifted(m_rgb.width, m_rgb.height);
    blitShifted(m_rgb, deviceDelta.x, deviceDelta.y, shifted);
    m_rgb         = std::move(shifted);
    m_bitmapDirty = true;
    changeView(viewport().panned(deviceDelta.x, deviceDelta.y).view());
}

void FractalCanvas::finishRubberBand(wxPoint at)
{
    const wxRect     logical(m_dragStart, at);
    const PixelPoint topLeft     = toDevice(logical.GetTopLeft());
    const PixelPoint bottomRight = toDevice(logical.GetBottomRight() + wxPoint(1, 1));
    const PixelRect  rect{topLeft.x, topLeft.y, bottomRight.x - topLeft.x,
                          bottomRight.y - topLeft.y};
    changeView(viewport().zoomedToRect(rect).view());
}

void FractalCanvas::onLeave(wxMouseEvent& /*event*/)
{
    if (onPointerMoved)
    {
        onPointerMoved(std::nullopt);
    }
    if (!m_orbit.empty())
    {
        m_orbit.clear();
        Refresh(false);
    }
}

void FractalCanvas::onCaptureLost(wxMouseCaptureLostEvent& /*event*/)
{
    m_drag      = Drag::None;
    m_panOffset = wxPoint();
    Refresh(false);
}

void FractalCanvas::updateOrbit(wxPoint at)
{
    const Viewport             vp = viewport();
    const std::vector<Complex> points =
        orbit(m_settings.fractal, complexAt(at),
              std::min(effectiveIterations(m_settings), kMaxOrbitPoints));
    m_orbit.clear();
    m_orbit.reserve(points.size());
    for (const Complex z : points)
    {
        const wxPoint p = toLogical(vp.toPixel(z));
        m_orbit.emplace_back(clampCoordinate(p.x), clampCoordinate(p.y));
    }
    Refresh(false);
}

// ---------------------------------------------------------------------------------------------------------------
// Keyboard

void FractalCanvas::onKeyDown(wxKeyEvent& event)
{
    const PixelSize size  = renderSize();
    const int       stepX = std::max(1, static_cast<int>(size.width * kKeyPanFraction));
    const int       stepY = std::max(1, static_cast<int>(size.height * kKeyPanFraction));
    switch (event.GetKeyCode())
    {
        case WXK_LEFT:
            changeView(viewport().panned(stepX, 0).view());
            break;
        case WXK_RIGHT:
            changeView(viewport().panned(-stepX, 0).view());
            break;
        case WXK_UP:
            changeView(viewport().panned(0, stepY).view());
            break;
        case WXK_DOWN:
            changeView(viewport().panned(0, -stepY).view());
            break;
        case '+':
        case '=':
        case WXK_NUMPAD_ADD:
        case WXK_PAGEUP:
            zoomAtCenter(kKeyZoomFactor);
            break;
        case '-':
        case WXK_NUMPAD_SUBTRACT:
        case WXK_PAGEDOWN:
            zoomAtCenter(1.0 / kKeyZoomFactor);
            break;
        case WXK_HOME:
            resetView();
            break;
        case WXK_ESCAPE:
            if (m_drag != Drag::None)
            {
                m_drag      = Drag::None;
                m_panOffset = wxPoint();
                if (HasCapture())
                {
                    ReleaseMouse();
                }
                Refresh(false);
            }
            break;
        default:
            event.Skip();
            break;
    }
}

}  // namespace mandelbrotter::gui
