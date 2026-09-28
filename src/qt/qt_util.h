#pragma once

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <filesystem>
#include <string>
#include <string_view>

#include "Mandelbrotter/app/ui_text.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"

namespace mandelbrotter::qt
{

/// All text crosses the Qt boundary as UTF-8.
[[nodiscard]] QString     toQt(std::string_view text);
[[nodiscard]] std::string fromQt(const QString& text);
/// File names, without a detour through the local 8-bit encoding.
[[nodiscard]] QString               fromPath(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path toPath(const QString& text);

/// A copy of `image` (RGB888) whose device pixel ratio is `dpr`, so it paints at `size / dpr`
/// logical pixels. A null image for an empty one.
[[nodiscard]] QImage toQImage(const RgbImage& image, double dpr);

/// Points, sizes and rectangles in pixels, as the app layer and Qt spell them.
[[nodiscard]] QPoint     toQt(PixelPoint point);
[[nodiscard]] PixelPoint fromQt(QPoint point);
[[nodiscard]] QRect      toQt(PixelRect rect);
[[nodiscard]] PixelRect  fromQt(const QRect& rect);
[[nodiscard]] QSize      toQt(PixelSize size);
[[nodiscard]] PixelSize  fromQt(QSize size);

[[nodiscard]] QColor toQt(app::Rgba color);

}  // namespace mandelbrotter::qt
