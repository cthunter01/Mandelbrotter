#include "qt/qt_util.h"

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/app/ui_text.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"

namespace
{

using mandelbrotter::PixelPoint;
using mandelbrotter::PixelRect;
using mandelbrotter::PixelSize;
using mandelbrotter::RgbImage;
namespace qt = mandelbrotter::qt;

TEST(QtUtil, TextRoundTripsAsUtf8)
{
    const std::string text =
        "Zoom 1\xC3\x97 \xE2\x88\x92 \xF0\x9F\x8C\x80";  // "Zoom 1x - " and a spiral
    const QString converted = qt::toQt(text);
    EXPECT_EQ(converted.size(), 12);  // the spiral takes two UTF-16 units
    EXPECT_EQ(converted.at(6), QChar(0x00D7));
    EXPECT_EQ(qt::fromQt(converted), text);
    EXPECT_TRUE(qt::toQt(std::string()).isEmpty());
}

TEST(QtUtil, PathsRoundTrip)
{
    const std::filesystem::path path = std::filesystem::path("dir") / "b\xC3\xB6kmarks.json";
    EXPECT_EQ(qt::toPath(qt::fromPath(path)), path);
}

TEST(QtUtil, ImagesAreCopiedWithTheirPixelRatio)
{
    RgbImage image(3, 2);
    image.set(0, 0, {.r = 255, .g = 0, .b = 0});
    image.set(2, 1, {.r = 1, .g = 2, .b = 3});
    const QImage converted = qt::toQImage(image, 2.0);
    ASSERT_FALSE(converted.isNull());
    EXPECT_EQ(converted.size(), QSize(3, 2));
    EXPECT_DOUBLE_EQ(converted.devicePixelRatio(), 2.0);
    EXPECT_EQ(converted.pixelColor(0, 0), QColor(255, 0, 0));
    EXPECT_EQ(converted.pixelColor(2, 1), QColor(1, 2, 3));
    EXPECT_EQ(converted.pixelColor(1, 1), QColor(0, 0, 0));
    EXPECT_TRUE(qt::toQImage(RgbImage(), 1.0).isNull());
}

TEST(QtUtil, GeometryConverts)
{
    EXPECT_EQ(qt::toQt(PixelPoint{3, -4}), QPoint(3, -4));
    EXPECT_EQ(qt::fromQt(QPoint(3, -4)), (PixelPoint{3, -4}));
    EXPECT_EQ(qt::toQt(PixelRect{1, 2, 30, 40}), QRect(1, 2, 30, 40));
    EXPECT_EQ(qt::fromQt(QRect(1, 2, 30, 40)), (PixelRect{1, 2, 30, 40}));
    EXPECT_EQ(qt::toQt(PixelSize{640, 400}), QSize(640, 400));
    EXPECT_EQ(qt::fromQt(QSize(640, 400)), (PixelSize{640, 400}));
    EXPECT_EQ(qt::toQt(mandelbrotter::app::kOrbitColor), QColor(255, 255, 255, 200));
}

}  // namespace
