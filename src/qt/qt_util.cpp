#include "qt/qt_util.h"

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QString>
#include <filesystem>
#include <string>
#include <string_view>

namespace mandelbrotter::qt
{

QString toQt(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

std::string fromQt(const QString& text)
{
    return text.toUtf8().toStdString();
}

QString fromPath(const std::filesystem::path& path)
{
    return QString::fromStdU16String(path.u16string());
}

std::filesystem::path toPath(const QString& text)
{
    return {text.toStdU16String()};
}

QImage toQImage(const RgbImage& image, double dpr)
{
    if (image.size().empty())
    {
        return {};
    }
    // A view of the packed rows, then a deep copy with Qt's own (aligned) row stride.
    QImage copy = QImage(image.pixels.data(), image.width, image.height,
                         static_cast<qsizetype>(image.width) * 3, QImage::Format_RGB888)
                      .copy();
    copy.setDevicePixelRatio(dpr);
    return copy;
}

QPoint toQt(PixelPoint point)
{
    return {point.x, point.y};
}

PixelPoint fromQt(QPoint point)
{
    return {point.x(), point.y()};
}

QRect toQt(PixelRect rect)
{
    return {rect.x, rect.y, rect.width, rect.height};
}

PixelRect fromQt(const QRect& rect)
{
    return {rect.x(), rect.y(), rect.width(), rect.height()};
}

QSize toQt(PixelSize size)
{
    return {size.width, size.height};
}

PixelSize fromQt(QSize size)
{
    return {size.width(), size.height()};
}

QColor toQt(app::Rgba color)
{
    return {color.red, color.green, color.blue, color.alpha};
}

}  // namespace mandelbrotter::qt
