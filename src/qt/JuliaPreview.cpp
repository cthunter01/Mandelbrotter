#include "qt/JuliaPreview.h"

#include <QFrame>
#include <QImage>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <optional>

#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/ProgressiveRenderer.h"
#include "Mandelbrotter/app/panel_model.h"
#include "Mandelbrotter/app/ui_text.h"
#include "Mandelbrotter/image.h"
#include "qt/qt_util.h"

namespace mandelbrotter::qt
{

JuliaPreview::JuliaPreview(QWidget* parent) : QFrame(parent)
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(toQt(app::kPreviewMinSize));
    m_timer.setSingleShot(true);
    m_timer.setInterval(app::kPreviewThrottle);
    connect(&m_timer, &QTimer::timeout, this, [this] { render(); });
}

void JuliaPreview::setFractal(const FractalSpec& spec)
{
    if (m_spec.family == spec.family && m_spec.exponent == spec.exponent)
    {
        return;
    }
    m_spec.family   = spec.family;
    m_spec.exponent = spec.exponent;
    schedule();
}

void JuliaPreview::setColoring(const ColoringSettings& coloring)
{
    if (m_coloring == coloring)
    {
        return;
    }
    m_coloring = coloring;
    schedule();
}

void JuliaPreview::setSeed(std::optional<Complex> seed)
{
    if (m_seed == seed)
    {
        return;
    }
    m_seed = seed;
    schedule();
}

void JuliaPreview::schedule()
{
    if (!m_timer.isActive())
    {
        m_timer.start();
    }
}

void JuliaPreview::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    schedule();
}

void JuliaPreview::render()
{
    const QSize area = contentsRect().size();
    if (!m_seed || area.isEmpty())
    {
        m_image = QImage();
        update();
        return;
    }
    const double    ratio = devicePixelRatio();
    const PixelSize size{std::max(1, static_cast<int>(std::lround(area.width() * ratio))),
                         std::max(1, static_cast<int>(std::lround(area.height() * ratio)))};
    const auto      buffer = renderSync(app::previewSettings(m_spec, *m_seed, m_coloring), size);
    if (!buffer)
    {
        return;
    }
    m_image = toQImage(colorized(*buffer, paletteOrDefault(m_coloring.palette), m_coloring), ratio);
    update();
}

void JuliaPreview::paintEvent(QPaintEvent* event)
{
    {
        QPainter painter(this);
        painter.fillRect(contentsRect(), Qt::black);
        if (!m_image.isNull())
        {
            painter.drawImage(contentsRect().topLeft(), m_image);
        }
    }
    QFrame::paintEvent(event);  // the sunken frame around it
}

}  // namespace mandelbrotter::qt
