#pragma once

#include <QEvent>
#include <QFocusEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPointF>
#include <QResizeEvent>
#include <QSizeF>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>
#include <cstdint>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/app/CanvasController.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::qt
{

/// The fractal view on screen: paints what its app::CanvasController holds and translates Qt's
/// mouse, keyboard and size events into the controller's calls. The controller does the rest
/// (rendering, navigation, the orbit overlay); the application talks to it through controller().
///
/// At an integer pixel ratio (1, 2) the controller works in logical pixels, as on wxGTK and wxOSX.
/// At a fractional one (Windows at 125%, fractional desktop scaling) the canvas hands it device
/// pixels at scale 1, as wxMSW does, and paints scaled down: the picture then maps one to one onto
/// the screen's pixels, and a pan never jumps by rounding.
class FractalCanvas : public QWidget
{
public:
    FractalCanvas(QWidget* parent, RenderSettings initial);
    ~FractalCanvas() override;
    FractalCanvas(const FractalCanvas&)            = delete;
    FractalCanvas& operator=(const FractalCanvas&) = delete;
    FractalCanvas(FractalCanvas&&)                 = delete;
    FractalCanvas& operator=(FractalCanvas&&)      = delete;

    [[nodiscard]] app::CanvasController&       controller() noexcept { return m_controller; }
    [[nodiscard]] const app::CanvasController& controller() const noexcept { return m_controller; }
    /// True while the controller works in device pixels (a fractional pixel ratio).
    [[nodiscard]] bool usesDevicePixels() const noexcept { return m_devicePixels; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// Tells the controller the size and scale, by the pixel-ratio rule above.
    void updateSize();
    /// A position in the widget, in the controller's pixels.
    [[nodiscard]] PixelPoint toController(QPointF position) const;
    /// The widget's area in the painter's coordinates (device pixels at a fractional ratio).
    [[nodiscard]] QSizeF paintArea() const;
    void                 drawOverlays(QPainter& painter) const;

    app::CanvasController m_controller;
    QTimer                m_resizeTimer;
    QImage                m_image;
    std::uint64_t         m_imageVersion{0};  ///< the controller's imageVersion() m_image shows
    double                m_ratio{1.0};       ///< the device pixel ratio last seen
    bool                  m_devicePixels{false};
};

}  // namespace mandelbrotter::qt
