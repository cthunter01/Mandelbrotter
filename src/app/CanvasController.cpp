#include "Mandelbrotter/app/CanvasController.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/ProgressiveRenderer.h"
#include "Mandelbrotter/ReferenceOrbit.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"
#include "Mandelbrotter/kernel.h"

namespace mandelbrotter::app
{

namespace
{

int clampCoordinate(int value)
{
    return std::clamp(value, -kOverlayCoordinateLimit, kOverlayCoordinateLimit);
}

PixelPoint difference(PixelPoint a, PixelPoint b)
{
    return {a.x - b.x, a.y - b.y};
}

/// The rectangle from `a` to `b`, both corners included, as wxRect(a, b) spans it.
PixelRect spanning(PixelPoint a, PixelPoint b)
{
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::abs(b.x - a.x) + 1,
            std::abs(b.y - a.y) + 1};
}

}  // namespace

CanvasController::CanvasController(RenderSettings initial, Hooks hooks)
  : m_hooks(std::move(hooks)), m_settings(std::move(initial))
{
}

CanvasController::~CanvasController()
{
    m_renderer.cancel();
}

// ---------------------------------------------------------------------------------------------------------------
// Hooks

void CanvasController::post(std::function<void()> work) const
{
    if (m_hooks.post)
    {
        m_hooks.post(std::move(work));
    }
}

void CanvasController::repaint() const
{
    if (m_hooks.requestRepaint)
    {
        m_hooks.requestRepaint();
    }
}

void CanvasController::captureMouse(bool on) const
{
    if (m_hooks.captureMouse)
    {
        m_hooks.captureMouse(on);
    }
}

void CanvasController::notifyUserInput() const
{
    if (onUserInput)
    {
        onUserInput();
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Geometry

void CanvasController::setSize(PixelSize logical, double scale)
{
    m_logicalSize = logical;
    m_scale       = scale;
}

void CanvasController::resizeSettled()
{
    if (m_iterations.size() != renderSize())
    {
        startRender();
    }
}

PixelSize CanvasController::renderSize() const noexcept
{
    return {std::max(1, static_cast<int>(std::lround(m_logicalSize.width * m_scale))),
            std::max(1, static_cast<int>(std::lround(m_logicalSize.height * m_scale)))};
}

Viewport CanvasController::viewport() const
{
    return {m_settings.view, renderSize()};
}

PixelPoint CanvasController::toDevice(PixelPoint logical) const noexcept
{
    return {static_cast<int>(std::floor(logical.x * m_scale)),
            static_cast<int>(std::floor(logical.y * m_scale))};
}

PixelPoint CanvasController::toLogical(PixelPoint device) const noexcept
{
    return {static_cast<int>(std::lround(device.x / m_scale)),
            static_cast<int>(std::lround(device.y / m_scale))};
}

Complex CanvasController::complexAt(PixelPoint logical) const
{
    return viewport().pixelCenter(toDevice(logical));
}

BigComplex CanvasController::bigAt(PixelPoint logical) const
{
    return viewport().pixelCenterBig(toDevice(logical));
}

// ---------------------------------------------------------------------------------------------------------------
// The model

void CanvasController::setSettings(const RenderSettings& settings)
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

void CanvasController::setPickSeedMode(bool enabled)
{
    m_pickSeedMode = enabled;
    if (m_hooks.setCursor)
    {
        m_hooks.setCursor(enabled ? CanvasCursor::BULLSEYE : CanvasCursor::CROSS);
    }
}

void CanvasController::setShowOrbit(bool enabled)
{
    m_showOrbit = enabled;
    if (!enabled)
    {
        m_orbit.clear();
    }
    repaint();
}

void CanvasController::resetView()
{
    changeView(defaultView(m_settings.fractal));
}

void CanvasController::zoomAtCenter(double factor)
{
    const PixelSize size = renderSize();
    changeView(viewport().zoomedAt({size.width / 2, size.height / 2}, factor).view());
}

void CanvasController::setHighlighted(bool on)
{
    if (m_highlighted != on)
    {
        m_highlighted = on;
        repaint();
    }
}

void CanvasController::showOrbitAt(Complex point)
{
    const Viewport vp = viewport();
    m_pinnedOrbit     = true;
    updateOrbit(toLogical(vp.toPixel(point)));
    if (onPointerMoved)
    {
        onPointerMoved(BigComplex::fromComplex(point, fractionBitsFor(vp.view().zoom)));
    }
}

void CanvasController::clearPinnedOrbit()
{
    if (!m_pinnedOrbit)
    {
        return;
    }
    m_pinnedOrbit = false;
    m_orbit.clear();
    repaint();
    if (onPointerMoved)
    {
        onPointerMoved(std::nullopt);  // as if the mouse had left
    }
}

void CanvasController::cancelRender()
{
    m_renderer.cancel();
}

// ---------------------------------------------------------------------------------------------------------------
// Rendering

void CanvasController::changeView(const ViewSpec& view)
{
    m_settings.view = view;
    startRender();
    if (onViewChanged)
    {
        onViewChanged(view);
    }
}

void CanvasController::startRender()
{
    const PixelSize size = renderSize();
    if (m_iterations.size() != size)
    {
        m_iterations = IterationBuffer(size.width, size.height);
    }
    if (m_rgb.size() != size)
    {
        m_rgb = RgbImage(size.width, size.height);
        ++m_imageVersion;
    }

    RenderJob job;
    job.settings = m_settings;
    job.size     = size;
    m_coarsePass = false;
    m_firstPass  = job.passes.empty() ? 1 : job.passes.front();
    m_firstPassTilesLeft =
        tileGrid({0, 0, size.width, size.height}, job.tileSize).size();  // the passes run in order
    const std::weak_ptr<const bool> alive = m_alive;
    m_generation                          = m_renderer.start(
        std::move(job),
        [this, alive](const TileResult& tile) {
            auto copy = std::make_shared<const TileResult>(tile);
            post([this, alive, copy] {
                if (alive.lock())
                {
                    applyTile(*copy);
                }
            });
        },
        [this, alive](const RenderCompletion& completion) {
            post([this, alive, completion] {
                if (alive.lock())
                {
                    finishRender(completion);
                }
            });
        });
    reportStatus(true, {});
}

void CanvasController::applyTile(const TileResult& tile)
{
    if (tile.generation != m_generation)
    {
        return;
    }
    if (tile.pass == m_firstPass && m_firstPassTilesLeft > 0)
    {
        --m_firstPassTilesLeft;
    }
    if (tile.pass != m_firstPass || m_firstPassTilesLeft == 0)
    {
        m_coarsePass = true;
    }
    tile.copyInto(m_iterations);
    colorize(m_iterations, tile.rect, paletteOrDefault(m_settings.coloring.palette),
             m_settings.coloring, m_rgb);
    ++m_imageVersion;
    repaint();
}

void CanvasController::finishRender(const RenderCompletion& completion)
{
    if (completion.generation != m_generation || completion.canceled)
    {
        return;
    }
    m_coarsePass = true;
    reportStatus(false, completion.elapsed);
}

void CanvasController::recolor()
{
    if (m_iterations.size().empty())
    {
        return;
    }
    colorize(m_iterations, paletteOrDefault(m_settings.coloring.palette), m_settings.coloring,
             m_rgb);
    ++m_imageVersion;
    repaint();
}

void CanvasController::reportStatus(bool rendering, std::chrono::milliseconds elapsed)
{
    if (onRenderStatus)
    {
        onRenderStatus({.rendering  = rendering,
                        .elapsed    = elapsed,
                        .iterations = effectiveIterations(m_settings)});
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Mouse

void CanvasController::wheel(PixelPoint at, double notches)
{
    notifyUserInput();
    const double factor = std::pow(kWheelZoomPerNotch, notches);
    changeView(viewport().zoomedAt(toDevice(at), factor).view());
}

void CanvasController::buttonDown(MouseButton button, PixelPoint at, bool shift)
{
    notifyUserInput();
    if (button == MouseButton::RIGHT)
    {
        beginDrag(Drag::RUBBER_BAND, at);
        return;
    }
    if (m_pickSeedMode)
    {
        if (onSeedPicked)
        {
            onSeedPicked(complexAt(at));
        }
        return;
    }
    beginDrag(shift ? Drag::RUBBER_BAND : Drag::PAN, at);
}

void CanvasController::buttonUp(MouseButton button, PixelPoint at)
{
    if (m_drag == Drag::NONE)
    {
        return;
    }
    endDrag(at, button == MouseButton::RIGHT);
}

void CanvasController::beginDrag(Drag kind, PixelPoint at)
{
    m_drag        = kind;
    m_dragStart   = at;
    m_dragCurrent = at;
    m_panOffset   = {};
    captureMouse(true);
}

void CanvasController::pointerMoved(PixelPoint at)
{
    if (m_drag == Drag::PAN)
    {
        m_panOffset = difference(at, m_dragStart);
        repaint();
    }
    else if (m_drag == Drag::RUBBER_BAND)
    {
        m_dragCurrent = at;
        repaint();
    }
    m_pinnedOrbit = false;
    if (onPointerMoved)
    {
        onPointerMoved(bigAt(at));
    }
    if (m_showOrbit && m_drag == Drag::NONE)
    {
        updateOrbit(at);
    }
}

void CanvasController::endDrag(PixelPoint at, bool rightButton)
{
    captureMouse(false);
    const Drag kind        = m_drag;
    m_drag                 = Drag::NONE;
    const PixelPoint delta = difference(at, m_dragStart);
    const bool moved = std::abs(delta.x) > kDragThresholdPx || std::abs(delta.y) > kDragThresholdPx;
    if (kind == Drag::PAN)
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
        repaint();
    }
}

void CanvasController::finishPan(PixelPoint at)
{
    const PixelPoint delta = difference(at, m_dragStart);
    m_panOffset            = {};
    if (delta == PixelPoint{})
    {
        repaint();
        return;
    }
    const PixelPoint deviceDelta = toDevice(delta);
    // Keep the old picture, shifted, as the placeholder until the new render's first pass lands.
    RgbImage shifted(m_rgb.width, m_rgb.height);
    blitShifted(m_rgb, deviceDelta.x, deviceDelta.y, shifted);
    m_rgb = std::move(shifted);
    ++m_imageVersion;
    changeView(viewport().panned(deviceDelta.x, deviceDelta.y).view());
}

void CanvasController::finishRubberBand(PixelPoint at)
{
    const PixelRect  logical     = spanning(m_dragStart, at);
    const PixelPoint topLeft     = toDevice({logical.x, logical.y});
    const PixelPoint bottomRight = toDevice({logical.right(), logical.bottom()});
    const PixelRect  rect{topLeft.x, topLeft.y, bottomRight.x - topLeft.x,
                          bottomRight.y - topLeft.y};
    changeView(viewport().zoomedToRect(rect).view());
}

void CanvasController::pointerLeft()
{
    if (m_pinnedOrbit)
    {
        return;  // showOrbitAt() keeps its orbit until the mouse moves again
    }
    if (onPointerMoved)
    {
        onPointerMoved(std::nullopt);
    }
    if (!m_orbit.empty())
    {
        m_orbit.clear();
        repaint();
    }
}

void CanvasController::captureLost()
{
    m_drag      = Drag::NONE;
    m_panOffset = {};
    repaint();
}

std::optional<PixelRect> CanvasController::rubberBand() const noexcept
{
    if (m_drag != Drag::RUBBER_BAND)
    {
        return std::nullopt;
    }
    return spanning(m_dragStart, m_dragCurrent);
}

void CanvasController::updateOrbit(PixelPoint at)
{
    const Viewport       vp        = viewport();
    const int            maxPoints = std::min(effectiveIterations(m_settings), kMaxOrbitPoints);
    const bool           deep      = usesPerturbation(vp.view().zoom);
    std::vector<Complex> points;
    if (deep)
    {
        // Doubles cannot place the pointer this deep, but its exact orbit can be computed; it
        // leaves the view within a few steps anyway.
        const ReferenceOrbit exact(m_settings.fractal, bigAt(at), maxPoints - 1);
        points.assign(exact.points().begin(), exact.points().end());
    }
    else
    {
        points = mandelbrotter::orbit(m_settings.fractal, complexAt(at), maxPoints);
    }
    m_orbit.clear();
    m_orbit.reserve(points.size());
    for (const Complex z : points)
    {
        const PixelPoint p = toLogical(vp.toPixel(z));
        m_orbit.push_back({clampCoordinate(p.x), clampCoordinate(p.y)});
    }
    if (deep && m_settings.fractal.julia && !m_orbit.empty())
    {
        m_orbit.front() = at;  // z0 is the pointer itself, which toPixel(Complex) cannot resolve
    }
    repaint();
}

// ---------------------------------------------------------------------------------------------------------------
// Keyboard

bool CanvasController::key(CanvasKey key)
{
    const PixelSize size  = renderSize();
    const int       stepX = std::max(1, static_cast<int>(size.width * kKeyPanFraction));
    const int       stepY = std::max(1, static_cast<int>(size.height * kKeyPanFraction));
    switch (key)
    {
        case CanvasKey::LEFT:
            changeView(viewport().panned(stepX, 0).view());
            break;
        case CanvasKey::RIGHT:
            changeView(viewport().panned(-stepX, 0).view());
            break;
        case CanvasKey::UP:
            changeView(viewport().panned(0, stepY).view());
            break;
        case CanvasKey::DOWN:
            changeView(viewport().panned(0, -stepY).view());
            break;
        case CanvasKey::ZOOM_IN:
            zoomAtCenter(kKeyZoomFactor);
            break;
        case CanvasKey::ZOOM_OUT:
            zoomAtCenter(1.0 / kKeyZoomFactor);
            break;
        case CanvasKey::HOME:
            resetView();
            break;
        case CanvasKey::ESCAPE:
            if (m_drag != Drag::NONE)
            {
                m_drag      = Drag::NONE;
                m_panOffset = {};
                captureMouse(false);
                repaint();
            }
            break;
    }
    return true;
}

}  // namespace mandelbrotter::app
