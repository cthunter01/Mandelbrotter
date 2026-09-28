#pragma once

#include <QFrame>
#include <QImage>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QWidget>
#include <optional>

#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::qt
{

/// A small live thumbnail of the Julia set for a seed, re-rendered (throttled) as the seed changes.
class JuliaPreview : public QFrame
{
public:
    explicit JuliaPreview(QWidget* parent);

    /// The family and exponent to preview (the Julia flag and seed are ignored).
    void setFractal(const FractalSpec& spec);
    void setColoring(const ColoringSettings& coloring);
    /// nullopt clears the preview.
    void setSeed(std::optional<Complex> seed);

    [[nodiscard]] const std::optional<Complex>& seed() const noexcept { return m_seed; }
    /// The picture shown (null when cleared).
    [[nodiscard]] const QImage& picture() const noexcept { return m_image; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    /// At most one render per kPreviewThrottle: a trailing throttle, not a debounce.
    void schedule();
    void render();

    FractalSpec            m_spec;
    ColoringSettings       m_coloring;
    std::optional<Complex> m_seed;
    QTimer                 m_timer;
    QImage                 m_image;
};

}  // namespace mandelbrotter::qt
