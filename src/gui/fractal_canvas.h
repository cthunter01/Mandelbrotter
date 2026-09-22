#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include <wx/bitmap.h>
#include <wx/timer.h>
#include <wx/window.h>

#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"
#include "Mandelbrotter/render_settings.h"
#include "Mandelbrotter/renderer.h"
#include "Mandelbrotter/viewport.h"

namespace mandelbrotter::gui
{

/// The fractal view: renders progressively on worker threads and handles all navigation.
///
/// The canvas owns the authoritative copy of the view (centre and zoom) while the user navigates
/// and reports changes through onViewChanged; everything else is pushed in with setSettings().
class FractalCanvas : public wxWindow
{
public:
    struct RenderStatus
    {
        bool                      rendering{};
        std::chrono::milliseconds elapsed{};
        int                       iterations{};
    };

    FractalCanvas(wxWindow* parent, RenderSettings initial);
    ~FractalCanvas() override;
    FractalCanvas(const FractalCanvas&)            = delete;
    FractalCanvas& operator=(const FractalCanvas&) = delete;
    FractalCanvas(FractalCanvas&&)                 = delete;
    FractalCanvas& operator=(FractalCanvas&&)      = delete;

    /// Re-renders if the geometry changed, otherwise just recolours.
    void                                setSettings(const RenderSettings& settings);
    [[nodiscard]] const RenderSettings& settings() const noexcept { return m_settings; }

    /// In pick mode a left click reports the complex number under the cursor via onSeedPicked
    /// instead of panning.
    void               setPickSeedMode(bool enabled);
    [[nodiscard]] bool pickSeedMode() const noexcept { return m_pickSeedMode; }
    void               setShowOrbit(bool enabled);
    [[nodiscard]] bool showOrbit() const noexcept { return m_showOrbit; }

    void resetView();
    void zoomAtCenter(double factor);

    /// The picture currently on screen (possibly mid-render).
    [[nodiscard]] const RgbImage& currentImage() const noexcept { return m_rgb; }

    std::function<void(const ViewSpec&)>        onViewChanged;
    std::function<void(std::optional<Complex>)> onPointerMoved;
    std::function<void(Complex)>                onSeedPicked;
    std::function<void(const RenderStatus&)>    onRenderStatus;

private:
    enum class Drag : std::uint8_t
    {
        None,
        Pan,
        RubberBand,
    };

    void onPaint(wxPaintEvent& event);
    void onSize(wxSizeEvent& event);
    void onResizeTimer(wxTimerEvent& event);
    void onMouseWheel(wxMouseEvent& event);
    void onLeftDown(wxMouseEvent& event);
    void onRightDown(wxMouseEvent& event);
    void onMotion(wxMouseEvent& event);
    void onMouseUp(wxMouseEvent& event);
    void onLeave(wxMouseEvent& event);
    void onCaptureLost(wxMouseCaptureLostEvent& event);
    void onKeyDown(wxKeyEvent& event);

    [[nodiscard]] double     scale() const;
    [[nodiscard]] PixelSize  renderSize() const;
    [[nodiscard]] Viewport   viewport() const;
    [[nodiscard]] PixelPoint toDevice(wxPoint p) const;
    [[nodiscard]] wxPoint    toLogical(PixelPoint p) const;
    [[nodiscard]] Complex    complexAt(wxPoint p) const;

    void changeView(const ViewSpec& view);
    void startRender();
    void applyTile(const TileResult& tile);
    void finishRender(const RenderCompletion& completion);
    void recolor();
    void reportStatus(bool rendering, std::chrono::milliseconds elapsed);

    void beginDrag(Drag kind, wxPoint at);
    void endDrag(wxPoint at, bool rightButton);
    void finishPan(wxPoint at);
    void finishRubberBand(wxPoint at);
    void updateOrbit(wxPoint at);
    void drawOverlays(wxDC& dc);

    RenderSettings      m_settings;
    ProgressiveRenderer m_renderer;
    std::uint64_t       m_generation{0};
    IterationBuffer     m_iterations;
    RgbImage            m_rgb;
    wxBitmap            m_bitmap;
    bool                m_bitmapDirty{false};
    wxTimer             m_resizeTimer;

    Drag                 m_drag{Drag::None};
    wxPoint              m_dragStart;
    wxPoint              m_dragCurrent;
    wxPoint              m_panOffset;
    bool                 m_pickSeedMode{false};
    bool                 m_showOrbit{false};
    std::vector<wxPoint> m_orbit;
};

}  // namespace mandelbrotter::gui
