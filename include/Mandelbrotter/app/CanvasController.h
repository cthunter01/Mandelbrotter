#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/ProgressiveRenderer.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"

namespace mandelbrotter::app
{

/// The numbers the help book documents for the canvas (navigating.html, orbit.html).
inline constexpr auto   kResizeDebounce         = std::chrono::milliseconds(150);
inline constexpr int    kDragThresholdPx        = 3;  ///< a shorter rubber band is a click
inline constexpr double kWheelZoomPerNotch      = 1.25;
inline constexpr double kKeyZoomFactor          = 2.0;  ///< also a right click's zoom out
inline constexpr double kKeyPanFraction         = 0.1;
inline constexpr int    kMaxOrbitPoints         = 512;
inline constexpr int    kOverlayCoordinateLimit = 10000;  ///< orbit points are clamped to this

/// The keys the canvas acts on; the toolkit maps its key codes onto them.
enum class CanvasKey : std::uint8_t
{
    LEFT,
    RIGHT,
    UP,
    DOWN,
    ZOOM_IN,   ///< +, =, keypad +, Page Up
    ZOOM_OUT,  ///< -, keypad -, Page Down
    HOME,
    ESCAPE,
};

enum class CanvasCursor : std::uint8_t
{
    CROSS,
    BULLSEYE,  ///< pick-seed mode
};

enum class MouseButton : std::uint8_t
{
    LEFT,
    RIGHT,
};

struct RenderStatus
{
    bool                      rendering{};
    std::chrono::milliseconds elapsed{};  ///< once finished
    int                       iterations{};
};

/// The fractal view's logic: renders progressively on worker threads and handles all navigation.
///
/// The controller owns the authoritative copy of the view (center and zoom) while the user
/// navigates and reports changes through onViewChanged; everything else is pushed in with
/// setSettings(). Every point and size crossing this API is in LOGICAL window pixels; the
/// controller converts to device pixels with the scale it was given (device = logical * scale).
///
/// Everything runs on the toolkit's thread. The renderer's callbacks come from worker threads and
/// are handed to the post hook, which must run them later on the toolkit's thread; a closure that
/// runs after the controller was destroyed does nothing.
class CanvasController
{
public:
    struct Hooks
    {
        std::function<void(std::function<void()>)>
                                          post;  ///< worker -> toolkit thread (wx: CallAfter)
        std::function<void()>             requestRepaint;
        std::function<void(CanvasCursor)> setCursor;
        std::function<void(bool)>         captureMouse;  ///< true captures, false releases
    };

    CanvasController(RenderSettings initial, Hooks hooks);
    /// Cancels the render.
    ~CanvasController();
    CanvasController(const CanvasController&)            = delete;
    CanvasController& operator=(const CanvasController&) = delete;
    CanvasController(CanvasController&&)                 = delete;
    CanvasController& operator=(CanvasController&&)      = delete;

    // ---- geometry
    /// The window's client size and content scale, from every size event. Does not render.
    void setSize(PixelSize logical, double scale);
    /// After the size has stopped changing for kResizeDebounce: renders if the picture's size no
    /// longer matches.
    void resizeSettled();

    // ---- the model
    /// Re-renders if the geometry changed, otherwise just recolors.
    void                                setSettings(const RenderSettings& settings);
    [[nodiscard]] const RenderSettings& settings() const noexcept { return m_settings; }
    /// In pick mode a left click reports the complex number under the cursor via onSeedPicked
    /// instead of panning.
    void               setPickSeedMode(bool enabled);
    [[nodiscard]] bool pickSeedMode() const noexcept { return m_pickSeedMode; }
    void               setShowOrbit(bool enabled);
    [[nodiscard]] bool showOrbit() const noexcept { return m_showOrbit; }
    void               resetView();
    void               zoomAtCenter(double factor);
    /// The picture currently on screen (possibly mid-render), in device pixels.
    [[nodiscard]] const RgbImage& image() const noexcept { return m_rgb; }
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
    /// Stops the workers (the toolkit calls it before tearing the window down).
    void cancelRender();

    // ---- input, in logical pixels
    void wheel(PixelPoint at, double notches);
    void buttonDown(MouseButton button, PixelPoint at, bool shift);
    void buttonUp(MouseButton button, PixelPoint at);
    void pointerMoved(PixelPoint at);
    void pointerLeft();
    /// The toolkit took the mouse capture away: the drag is abandoned.
    void captureLost();
    /// Returns false when the key is not the canvas's (the toolkit passes it on). ESCAPE is always
    /// handled: it abandons a drag, if any.
    bool key(CanvasKey key);

    // ---- what to paint
    /// Changes whenever image() does: the toolkit rebuilds its bitmap then.
    [[nodiscard]] std::uint64_t imageVersion() const noexcept { return m_imageVersion; }
    /// Where to draw the picture while a pan drag is in progress (logical pixels).
    [[nodiscard]] PixelPoint panOffset() const noexcept { return m_panOffset; }
    /// The rubber band being dragged, from the pixel pressed to the pixel under the pointer, both
    /// included (so it is never empty); nullopt when none.
    [[nodiscard]] std::optional<PixelRect> rubberBand() const noexcept;
    /// The orbit overlay's points, logical and clamped to +-kOverlayCoordinateLimit; drawn while
    /// showOrbit() holds and there are at least two.
    [[nodiscard]] std::span<const PixelPoint> orbit() const noexcept { return m_orbit; }
    [[nodiscard]] bool                        highlighted() const noexcept { return m_highlighted; }

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

    [[nodiscard]] PixelSize  renderSize() const noexcept;
    [[nodiscard]] Viewport   viewport() const;
    [[nodiscard]] PixelPoint toDevice(PixelPoint logical) const noexcept;
    [[nodiscard]] PixelPoint toLogical(PixelPoint device) const noexcept;
    [[nodiscard]] Complex    complexAt(PixelPoint logical) const;
    [[nodiscard]] BigComplex bigAt(PixelPoint logical) const;

    void changeView(const ViewSpec& view);
    void startRender();
    void applyTile(const TileResult& tile);
    void finishRender(const RenderCompletion& completion);
    void recolor();
    void reportStatus(bool rendering, std::chrono::milliseconds elapsed);

    void beginDrag(Drag kind, PixelPoint at);
    void endDrag(PixelPoint at, bool rightButton);
    void finishPan(PixelPoint at);
    void finishRubberBand(PixelPoint at);
    void updateOrbit(PixelPoint at);

    void post(std::function<void()> work) const;
    void repaint() const;
    void captureMouse(bool on) const;
    void notifyUserInput() const;

    Hooks          m_hooks;
    RenderSettings m_settings;
    PixelSize      m_logicalSize;
    double         m_scale{1.0};

    ProgressiveRenderer m_renderer;
    std::uint64_t       m_generation{0};
    IterationBuffer     m_iterations;
    RgbImage            m_rgb;
    std::uint64_t       m_imageVersion{0};

    Drag                    m_drag{Drag::NONE};
    PixelPoint              m_dragStart;
    PixelPoint              m_dragCurrent;
    PixelPoint              m_panOffset;
    bool                    m_pickSeedMode{false};
    bool                    m_showOrbit{false};
    bool                    m_pinnedOrbit{false};
    bool                    m_highlighted{false};
    std::vector<PixelPoint> m_orbit;

    bool        m_coarsePass{true};  ///< see hasCoarsePicture()
    int         m_firstPass{0};
    std::size_t m_firstPassTilesLeft{0};

    /// Posted closures hold a weak copy: once it has expired they do nothing.
    std::shared_ptr<const bool> m_alive = std::make_shared<const bool>(true);
};

}  // namespace mandelbrotter::app
