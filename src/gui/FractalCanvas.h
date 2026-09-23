#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include <wx/bitmap.h>
#include <wx/timer.h>
#include <wx/window.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/ProgressiveRenderer.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"

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

    /// The tour's accent border around the picture.
    void setHighlighted(bool on);
    /// Pins the orbit of `point` as if the mouse hovered there (the tour, screenshots) and reports
    /// it through onPointerMoved; the next mouse movement or clearPinnedOrbit() ends it.
    void showOrbitAt(Complex point);
    void clearPinnedOrbit();
    /// True once the render in progress has shown its coarsest pass (or finished): the moment a
    /// new frame can replace it without waiting (demo playback).
    [[nodiscard]] bool hasCoarsePicture() const noexcept { return m_coarsePass; }
    [[nodiscard]] bool rendering() const noexcept { return m_renderer.busy(); }

    std::function<void(const ViewSpec&)>                  onViewChanged;
    std::function<void(const std::optional<BigComplex>&)> onPointerMoved;
    std::function<void(Complex)>                          onSeedPicked;
    std::function<void(const RenderStatus&)>              onRenderStatus;
    /// Any mouse button or wheel input, reported before it is acted on (demos stop on it).
    std::function<void()> onUserInput;

private:
    enum class Drag : std::uint8_t
    {
        NONE,
        PAN,
        RUBBER_BAND,
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
    [[nodiscard]] BigComplex bigAt(wxPoint p) const;

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
    void notifyUserInput() const;

    RenderSettings      m_settings;
    ProgressiveRenderer m_renderer;
    std::uint64_t       m_generation{0};
    IterationBuffer     m_iterations;
    RgbImage            m_rgb;
    wxBitmap            m_bitmap;
    bool                m_bitmapDirty{false};
    wxTimer             m_resizeTimer;

    Drag                 m_drag{Drag::NONE};
    wxPoint              m_dragStart;
    wxPoint              m_dragCurrent;
    wxPoint              m_panOffset;
    bool                 m_pickSeedMode{false};
    bool                 m_showOrbit{false};
    bool                 m_pinnedOrbit{false};
    bool                 m_highlighted{false};
    std::vector<wxPoint> m_orbit;

    bool        m_coarsePass{true};  ///< see hasCoarsePicture()
    int         m_firstPass{0};
    std::size_t m_firstPassTilesLeft{0};
};

}  // namespace mandelbrotter::gui
