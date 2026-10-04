#include "qt/FractalCanvas.h"

#include <QBrush>
#include <QColor>
#include <QCursor>
#include <QEvent>
#include <QFocusEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPoint>
#include <QPointF>
#include <QPolygon>
#include <QRect>
#include <QRectF>
#include <QResizeEvent>
#include <QSizeF>
#include <QString>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>
#include <cmath>
#include <functional>
#include <optional>
#include <span>
#include <utility>

#include "Mandelbrotter/app/CanvasController.h"
#include "Mandelbrotter/app/ui_text.h"
#include "Mandelbrotter/geometry.h"
#include "qt/cursors.h"
#include "qt/qt_util.h"

namespace mandelbrotter::qt
{

using namespace Qt::StringLiterals;

namespace
{

/// A wheel notch, in QWheelEvent::angleDelta() units (eighths of a degree).
constexpr double kWheelNotch = 120.0;

bool isInteger(double ratio) noexcept
{
    return std::abs(ratio - std::round(ratio)) < 1e-6;
}

std::optional<app::CanvasKey> canvasKey(const QKeyEvent& event)
{
    switch (event.key())
    {
        case Qt::Key_Left:
            return app::CanvasKey::LEFT;
        case Qt::Key_Right:
            return app::CanvasKey::RIGHT;
        case Qt::Key_Up:
            return app::CanvasKey::UP;
        case Qt::Key_Down:
            return app::CanvasKey::DOWN;
        case Qt::Key_Plus:  // keypad + too, with Qt::KeypadModifier
        case Qt::Key_Equal:
        case Qt::Key_PageUp:
            return app::CanvasKey::ZOOM_IN;
        case Qt::Key_Minus:
        case Qt::Key_PageDown:
            return app::CanvasKey::ZOOM_OUT;
        case Qt::Key_Home:
            return app::CanvasKey::HOME;
        case Qt::Key_Escape:
            return app::CanvasKey::ESCAPE;
        default:
            return std::nullopt;
    }
}

std::optional<app::MouseButton> mouseButton(Qt::MouseButton button)
{
    // Only left and right: the controller ends a drag on any button-up.
    switch (button)
    {
        case Qt::LeftButton:
            return app::MouseButton::LEFT;
        case Qt::RightButton:
            return app::MouseButton::RIGHT;
        default:
            return std::nullopt;
    }
}

}  // namespace

FractalCanvas::FractalCanvas(QWidget* parent, RenderSettings initial)
  : QWidget(parent),
    m_controller(std::move(initial),
                 {
                     .post =
                         [this](std::function<void()> work) {
                             // Thread-safe and never blocking; dropped once the canvas is gone.
                             QMetaObject::invokeMethod(this, std::move(work), Qt::QueuedConnection);
                         },
                     .requestRepaint = [this] { update(); },
                     .setCursor =
                         [this](app::CanvasCursor cursor) {
                             if (cursor == app::CanvasCursor::BULLSEYE)
                             {
                                 setCursor(bullseyeCursor());
                             }
                             else
                             {
                                 setCursor(Qt::CrossCursor);
                             }
                         },
                     // Qt grabs the mouse by itself between a press and its release.
                     .captureMouse = {},
                 })
{
    setObjectName(u"FractalCanvas"_s);
    // Without mouse tracking Qt reports moves only while a button is held: the pointer field, the
    // Julia preview and the orbit overlay would be dead.
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setContextMenuPolicy(Qt::PreventContextMenu);
    setMinimumSize(toQt(app::kCanvasMinSize));
    setCursor(Qt::CrossCursor);

    m_resizeTimer.setSingleShot(true);
    m_resizeTimer.setInterval(app::kResizeDebounce);
    connect(&m_resizeTimer, &QTimer::timeout, this, [this] { m_controller.resizeSettled(); });
    updateSize();
}

FractalCanvas::~FractalCanvas()
{
    // Stop the workers while their post target still exists.
    m_controller.cancelRender();
}

void FractalCanvas::updateSize()
{
    m_ratio                 = devicePixelRatio();
    const bool devicePixels = !isInteger(m_ratio);
    if (devicePixels != m_devicePixels)
    {
        m_devicePixels = devicePixels;
        m_imageVersion = 0;  // the picture's pixel ratio changes
    }
    if (m_devicePixels)
    {
        m_controller.setSize(
            {
                static_cast<int>(std::lround(width() * m_ratio)),
                static_cast<int>(std::lround(height() * m_ratio)),
            },
            1.0);
    }
    else
    {
        m_controller.setSize(fromQt(size()), m_ratio);
    }
}

QSizeF FractalCanvas::paintArea() const
{
    return m_devicePixels ? QSizeF(size()) * m_ratio : QSizeF(size());
}

PixelPoint FractalCanvas::toController(QPointF position) const
{
    if (m_devicePixels)
    {
        return {
            static_cast<int>(std::lround(position.x() * m_ratio)),
            static_cast<int>(std::lround(position.y() * m_ratio)),
        };
    }
    return fromQt(position.toPoint());
}

// ---------------------------------------------------------------------------------------------------------------
// Painting

void FractalCanvas::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    if (m_imageVersion != m_controller.imageVersion())
    {
        m_image        = toQImage(m_controller.image(), m_devicePixels ? 1.0 : m_ratio);
        m_imageVersion = m_controller.imageVersion();
    }
    if (m_devicePixels)
    {
        painter.scale(1.0 / m_ratio, 1.0 / m_ratio);  // everything below is in device pixels
    }
    const QSizeF area    = paintArea();
    const QSizeF picture = m_image.isNull() ? QSizeF() : m_image.deviceIndependentSize();
    const QPoint offset  = toQt(m_controller.panOffset());
    if (offset != QPoint() || picture.width() < area.width() || picture.height() < area.height())
    {
        painter.fillRect(QRectF(QPointF(), area), Qt::black);
    }
    if (!m_image.isNull())
    {
        painter.drawImage(offset, m_image);
    }
    drawOverlays(painter);
}

void FractalCanvas::drawOverlays(QPainter& painter) const
{
    // Pen widths are logical pixels; in device pixels they grow by the ratio.
    const double scale = m_devicePixels ? m_ratio : 1.0;
    if (const std::optional<PixelRect> band = m_controller.rubberBand())
    {
        // The band includes both corner pixels; a QRect outline covers one pixel more.
        const QRect outline = toQt(*band).adjusted(0, 0, -1, -1);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::black, app::kRubberBandWidth * scale));
        painter.drawRect(outline);
        QPen dashes(Qt::white, app::kRubberBandWidth * scale);
        dashes.setDashPattern({3.0, 3.0});  // wx's short dash
        painter.setPen(dashes);
        painter.drawRect(outline);
    }
    const std::span<const PixelPoint> orbit = m_controller.orbit();
    if (m_controller.showOrbit() && orbit.size() > 1)
    {
        QPolygon points;
        points.reserve(static_cast<qsizetype>(orbit.size()));
        for (const PixelPoint point : orbit)
        {
            points.append(toQt(point));
        }
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(toQt(app::kOrbitColor), app::kOrbitWidth * scale));
        painter.drawPolyline(points);
        painter.setPen(QPen(toQt(app::kOrbitStartColor), app::kOrbitStartWidth * scale));
        const double radius = app::kOrbitStartRadius * scale;
        painter.drawEllipse(QPointF(points.front()), radius, radius);
        painter.restore();
    }
    if (m_controller.highlighted())
    {
        const double width = app::kHighlightRingWidth * scale;
        const QRectF area(QPointF(), paintArea());
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(palette().color(QPalette::Highlight), width));
        painter.drawRect(area.adjusted(width / 2, width / 2, -width / 2, -width / 2));
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Size

void FractalCanvas::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateSize();
    m_resizeTimer.start();
}

void FractalCanvas::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    switch (event->type())
    {
        case QEvent::DevicePixelRatioChange:
            // Qt sends no resize when only the ratio changes, and only resizeSettled re-renders.
            updateSize();
            m_resizeTimer.start();
            break;
        case QEvent::ActivationChange:
            if (!isActiveWindow())
            {
                m_controller.captureLost();
            }
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Mouse

void FractalCanvas::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().y();
    if (delta != 0)
    {
        m_controller.wheel(toController(event->position()), delta / kWheelNotch);
    }
    event->accept();
}

void FractalCanvas::mousePressEvent(QMouseEvent* event)
{
    const std::optional<app::MouseButton> button = mouseButton(event->button());
    if (!button)
    {
        event->ignore();
        return;
    }
    setFocus(Qt::MouseFocusReason);
    m_controller.buttonDown(*button, toController(event->position()),
                            event->modifiers().testFlag(Qt::ShiftModifier));
}

void FractalCanvas::mouseMoveEvent(QMouseEvent* event)
{
    m_controller.pointerMoved(toController(event->position()));
}

void FractalCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    const std::optional<app::MouseButton> button = mouseButton(event->button());
    if (!button)
    {
        event->ignore();
        return;
    }
    m_controller.buttonUp(*button, toController(event->position()));
}

void FractalCanvas::leaveEvent(QEvent* /*event*/)
{
    m_controller.pointerLeft();
}

void FractalCanvas::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    m_controller.captureLost();  // harmless when nothing is being dragged
}

// ---------------------------------------------------------------------------------------------------------------
// Keyboard

void FractalCanvas::keyPressEvent(QKeyEvent* event)
{
    const std::optional<app::CanvasKey> key = canvasKey(*event);
    if (!key || !m_controller.key(*key))
    {
        event->ignore();
        return;
    }
    event->accept();
}

}  // namespace mandelbrotter::qt
