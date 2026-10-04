#include "qt/FractalCanvas.h"

#include <QApplication>
#include <QColor>
#include <QCursor>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QPointingDevice>
#include <QTest>
#include <QWheelEvent>
#include <cmath>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/CanvasController.h"
#include "Mandelbrotter/app/scenes.h"
#include "Mandelbrotter/geometry.h"
#include "QtHarness.h"
#include "TempDir.h"
#include "qt/MainWindow.h"
#include "qt/qt_util.h"

namespace
{

using mandelbrotter::Complex;
using mandelbrotter::PixelPoint;
using mandelbrotter::PixelSize;
using mandelbrotter::Viewport;
using mandelbrotter::ViewSpec;
using mandelbrotter::app::StatusField;
using mandelbrotter::test::pumpUntil;
using mandelbrotter::test::QtHarness;
namespace app = mandelbrotter::app;
namespace qt  = mandelbrotter::qt;

/// The canvas's size in the controller's pixels.
PixelSize devicePixels(const qt::FractalCanvas& canvas)
{
    const double ratio = canvas.devicePixelRatio();
    return {
        static_cast<int>(std::lround(canvas.width() * ratio)),
        static_cast<int>(std::lround(canvas.height() * ratio)),
    };
}

/// A shown main window whose picture has been rendered at the canvas's size (the first render, at
/// the canvas's initial size, is followed by another once the window's size has settled).
struct Rendered : QtHarness
{
    Rendered()
    {
        EXPECT_TRUE(QTest::qWaitForWindowExposed(&window));
        EXPECT_TRUE(pumpUntil([this] {
            return renderDone() && canvas().controller().image().size() == devicePixels(canvas());
        }));
    }

    [[nodiscard]] qt::FractalCanvas& canvas() const { return window.canvas(); }
    [[nodiscard]] const ViewSpec&    view() { return window.app().settings().view; }
    /// The view as the controller sees it, in its pixels.
    [[nodiscard]] Viewport viewport() { return {view(), canvas().controller().image().size()}; }
};

void wheel(qt::FractalCanvas& canvas, QPoint at, int angle)
{
    QWheelEvent event(QPointF(at), QPointF(canvas.mapToGlobal(at)), QPoint(), QPoint(0, angle),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&canvas, &event);
}

TEST(FractalCanvas, TheFirstPictureIsRendered)
{
    const Rendered h;
    EXPECT_EQ(h.canvas().controller().image().size(), devicePixels(h.canvas()));
    EXPECT_TRUE(h.canvas().hasFocus());
    EXPECT_TRUE(h.canvas().hasMouseTracking());
}

TEST(FractalCanvas, AWheelNotchKeepsThePointUnderTheCursor)
{
    Rendered         h;
    const QPoint     at(120, 90);
    const PixelPoint pixel  = qt::fromQt(at);
    const Complex    before = h.viewport().pixelCenter(pixel);
    wheel(h.canvas(), at, 120);
    EXPECT_DOUBLE_EQ(h.view().zoom, app::kWheelZoomPerNotch);
    const Complex after = h.viewport().pixelCenter(pixel);
    EXPECT_NEAR(after.re, before.re, 1e-9);
    EXPECT_NEAR(after.im, before.im, 1e-9);
    wheel(h.canvas(), at, -240);
    EXPECT_DOUBLE_EQ(h.view().zoom, 1.0 / app::kWheelZoomPerNotch);
}

TEST(FractalCanvas, ALeftDragPans)
{
    Rendered       h;
    const Viewport before = h.viewport();
    QTest::mousePress(&h.canvas(), Qt::LeftButton, {}, QPoint(100, 100));
    QTest::mouseMove(&h.canvas(), QPoint(110, 104));
    EXPECT_EQ(h.canvas().controller().panOffset(), (PixelPoint{10, 4}));
    QTest::mouseRelease(&h.canvas(), Qt::LeftButton, {}, QPoint(110, 104));
    EXPECT_EQ(h.view(), before.panned(10, 4).view());
}

TEST(FractalCanvas, ARightDragBeyondThreePixelsZoomsToTheRectangle)
{
    Rendered       h;
    const Viewport start = h.viewport();
    // Three pixels: still a click, which zooms out at the pointer.
    QTest::mousePress(&h.canvas(), Qt::RightButton, {}, QPoint(100, 100));
    QTest::mouseRelease(&h.canvas(), Qt::RightButton, {}, QPoint(103, 100));
    EXPECT_EQ(h.view(), start.zoomedAt({103, 100}, 1.0 / app::kKeyZoomFactor).view());

    const Viewport before = h.viewport();
    QTest::mousePress(&h.canvas(), Qt::RightButton, {}, QPoint(100, 100));
    QTest::mouseMove(&h.canvas(), QPoint(140, 130));
    EXPECT_TRUE(h.canvas().controller().rubberBand().has_value());
    QTest::mouseRelease(&h.canvas(), Qt::RightButton, {}, QPoint(140, 130));
    EXPECT_EQ(h.view(), before.zoomedToRect({100, 100, 41, 31}).view());
}

TEST(FractalCanvas, AShiftDragZoomsToTheRectangle)
{
    Rendered       h;
    const Viewport before = h.viewport();
    QTest::mousePress(&h.canvas(), Qt::LeftButton, Qt::ShiftModifier, QPoint(200, 150));
    QTest::mouseMove(&h.canvas(), QPoint(150, 120));
    QTest::mouseRelease(&h.canvas(), Qt::LeftButton, Qt::ShiftModifier, QPoint(150, 120));
    EXPECT_EQ(h.view(), before.zoomedToRect({150, 120, 51, 31}).view());
}

TEST(FractalCanvas, MovingTheMouseShowsThePointAndLeavingClearsIt)
{
    Rendered h;
    QTest::mouseMove(&h.canvas(), QPoint(50, 60));
    ASSERT_TRUE(pumpUntil([&] { return !h.status(StatusField::POINTER).empty(); }));
    const std::string first = h.status(StatusField::POINTER);
    QTest::mouseMove(&h.canvas(), QPoint(300, 200));
    ASSERT_TRUE(pumpUntil([&] { return h.status(StatusField::POINTER) != first; }));
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&h.canvas(), &leave);
    EXPECT_EQ(h.status(StatusField::POINTER), "");
}

TEST(FractalCanvas, KeysPanZoomAndReset)
{
    Rendered        h;
    const Viewport  start = h.viewport();
    const PixelSize size  = h.canvas().controller().image().size();
    const int       stepX = static_cast<int>(size.width * app::kKeyPanFraction);
    const int       stepY = static_cast<int>(size.height * app::kKeyPanFraction);
    QTest::keyClick(&h.canvas(), Qt::Key_Left);
    EXPECT_EQ(h.view(), start.panned(stepX, 0).view());
    QTest::keyClick(&h.canvas(), Qt::Key_Down);
    EXPECT_EQ(h.view(), start.panned(stepX, 0).panned(0, -stepY).view());
    QTest::keyClick(&h.canvas(), Qt::Key_Right);
    QTest::keyClick(&h.canvas(), Qt::Key_Up);
    QTest::keyClick(&h.canvas(), Qt::Key_Home);
    EXPECT_EQ(h.view(), start.view());

    QTest::keyClick(&h.canvas(), Qt::Key_Plus);
    EXPECT_DOUBLE_EQ(h.view().zoom, 2.0);
    QTest::keyClick(&h.canvas(), Qt::Key_Equal);
    EXPECT_DOUBLE_EQ(h.view().zoom, 4.0);
    QTest::keyClick(&h.canvas(), Qt::Key_Plus, Qt::KeypadModifier);
    EXPECT_DOUBLE_EQ(h.view().zoom, 8.0);
    QTest::keyClick(&h.canvas(), Qt::Key_PageUp);
    EXPECT_DOUBLE_EQ(h.view().zoom, 16.0);
    QTest::keyClick(&h.canvas(), Qt::Key_Minus);
    QTest::keyClick(&h.canvas(), Qt::Key_Minus, Qt::KeypadModifier);
    QTest::keyClick(&h.canvas(), Qt::Key_PageDown);
    EXPECT_DOUBLE_EQ(h.view().zoom, 2.0);
    QTest::keyClick(&h.canvas(), Qt::Key_Home);
    EXPECT_DOUBLE_EQ(h.view().zoom, 1.0);
}

TEST(FractalCanvas, EscapeAbandonsADrag)
{
    Rendered       h;
    const ViewSpec before = h.view();
    QTest::mousePress(&h.canvas(), Qt::LeftButton, {}, QPoint(100, 100));
    QTest::mouseMove(&h.canvas(), QPoint(130, 120));
    EXPECT_EQ(h.canvas().controller().panOffset(), (PixelPoint{30, 20}));
    QTest::keyClick(&h.canvas(), Qt::Key_Escape);
    EXPECT_EQ(h.canvas().controller().panOffset(), (PixelPoint{}));
    QTest::mouseRelease(&h.canvas(), Qt::LeftButton, {}, QPoint(130, 120));
    EXPECT_EQ(h.view(), before);
}

TEST(FractalCanvas, TheMiddleButtonDoesNothing)
{
    Rendered       h;
    const ViewSpec before = h.view();
    QTest::mouseClick(&h.canvas(), Qt::MiddleButton, {}, QPoint(100, 100));
    QTest::mousePress(&h.canvas(), Qt::MiddleButton, {}, QPoint(100, 100));
    QTest::mouseMove(&h.canvas(), QPoint(150, 150));
    QTest::mouseRelease(&h.canvas(), Qt::MiddleButton, {}, QPoint(150, 150));
    EXPECT_EQ(h.view(), before);
}

TEST(FractalCanvas, PickModeShowsTheBullseyeAndAClickPicksTheSeed)
{
    Rendered h;
    EXPECT_EQ(h.canvas().cursor().shape(), Qt::CrossCursor);
    h.window.app().setPickSeedMode(true);
    EXPECT_EQ(h.canvas().cursor().shape(), Qt::BitmapCursor);
    const Complex seed = h.viewport().pixelCenter({80, 70});
    QTest::mouseClick(&h.canvas(), Qt::LeftButton, {}, QPoint(80, 70));
    const auto& fractal = h.window.app().settings().fractal;
    EXPECT_TRUE(fractal.julia);
    EXPECT_NEAR(fractal.seed.re, seed.re, 1e-9);
    EXPECT_NEAR(fractal.seed.im, seed.im, 1e-9);
    EXPECT_FALSE(h.canvas().controller().pickSeedMode());
    EXPECT_EQ(h.canvas().cursor().shape(), Qt::CrossCursor);
}

TEST(FractalCanvas, AResizeRendersOnceWhenItSettles)
{
    Rendered h;
    int      renders                = 0;
    h.window.app().onRenderFinished = [&renders] { ++renders; };
    const PixelSize before          = h.canvas().controller().image().size();
    h.window.resize(h.window.width() - 100, h.window.height() - 60);
    QApplication::processEvents();
    EXPECT_EQ(h.canvas().controller().image().size(), before);  // not before the debounce
    ASSERT_TRUE(pumpUntil([&] { return renders > 0; }));
    QTest::qWait(static_cast<int>(app::kResizeDebounce.count()) * 2);
    EXPECT_EQ(renders, 1);
    EXPECT_EQ(h.canvas().controller().image().size(), devicePixels(h.canvas()));
    h.window.app().onRenderFinished = nullptr;
}

TEST(FractalCanvas, ClosingTheWindowMidRenderIsClean)
{
    const mandelbrotter::test::TempDir dir;
    for (int i = 0; i < 3; ++i)
    {
        auto window = std::make_unique<qt::MainWindow>(app::deepSeahorse(app::kTourDeepZoom),
                                                       dir / "bookmarks.json");
        window->show();
        window->resize(1600, 1000);
        QTest::qWait(static_cast<int>(app::kResizeDebounce.count()) + 20);  // a big render starts
        QApplication::processEvents();
        window->close();
        window.reset();  // posted tiles are still queued
        QApplication::processEvents();
    }
}

// ---------------------------------------------------------------------------------------------------------------
// At a fractional pixel ratio (run as qt.fractional_ratio, with QT_SCALE_FACTOR=1.5)

/// True when the canvas has a fractional pixel ratio; the tests below skip themselves otherwise.
bool fractional(const qt::FractalCanvas& canvas)
{
    const double ratio = canvas.devicePixelRatio();
    return std::abs(ratio - std::round(ratio)) > 1e-6;
}

TEST(FractionalRatio, TheControllerWorksInDevicePixels)
{
    const Rendered h;
    if (!fractional(h.canvas()))
    {
        GTEST_SKIP() << "needs a fractional pixel ratio (QT_SCALE_FACTOR=1.5)";
    }
    EXPECT_TRUE(h.canvas().usesDevicePixels());
    const double ratio = h.canvas().devicePixelRatio();
    EXPECT_EQ(h.canvas().controller().image().size(),
              (PixelSize{static_cast<int>(std::lround(h.canvas().width() * ratio)),
                         static_cast<int>(std::lround(h.canvas().height() * ratio))}));
}

TEST(FractionalRatio, APanMovesByDevicePixelsWithoutAJump)
{
    Rendered h;
    if (!fractional(h.canvas()))
    {
        GTEST_SKIP() << "needs a fractional pixel ratio (QT_SCALE_FACTOR=1.5)";
    }
    const double   ratio  = h.canvas().devicePixelRatio();
    const Viewport before = h.viewport();
    QTest::mousePress(&h.canvas(), Qt::LeftButton, {}, QPoint(100, 100));
    QTest::mouseMove(&h.canvas(), QPoint(110, 104));
    const PixelPoint offset{
        static_cast<int>(std::lround(110 * ratio) - std::lround(100 * ratio)),
        static_cast<int>(std::lround(104 * ratio) - std::lround(100 * ratio)),
    };
    EXPECT_EQ(h.canvas().controller().panOffset(), offset);
    QTest::mouseRelease(&h.canvas(), Qt::LeftButton, {}, QPoint(110, 104));
    // The view moves by exactly what the drag showed.
    EXPECT_EQ(h.view(), before.panned(offset.x, offset.y).view());
}

TEST(FractionalRatio, ThePictureFillsTheCanvas)
{
    const Rendered h;
    if (!fractional(h.canvas()))
    {
        GTEST_SKIP() << "needs a fractional pixel ratio (QT_SCALE_FACTOR=1.5)";
    }
    const QImage shown = h.canvas().grab().toImage();
    const QImage right = shown.copy(shown.width() - 3, 0, 3, shown.height());
    const QImage below = shown.copy(0, shown.height() - 3, shown.width(), 3);
    // The picture's corners are far outside the set: never the black of an unpainted edge.
    EXPECT_NE(right.pixelColor(1, 1), QColor(Qt::black));
    EXPECT_NE(below.pixelColor(below.width() - 2, 1), QColor(Qt::black));
}

}  // namespace
