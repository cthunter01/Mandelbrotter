#include "Mandelbrotter/app/CanvasController.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/ProgressiveRenderer.h"
#include "Mandelbrotter/ReferenceOrbit.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/app/scenes.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"
#include "Mandelbrotter/kernel.h"
#include "PostQueue.h"

namespace
{

using mandelbrotter::BigComplex;
using mandelbrotter::Complex;
using mandelbrotter::PixelPoint;
using mandelbrotter::PixelRect;
using mandelbrotter::PixelSize;
using mandelbrotter::RenderSettings;
using mandelbrotter::RgbImage;
using mandelbrotter::Viewport;
using mandelbrotter::ViewSpec;
using mandelbrotter::app::CanvasController;
using mandelbrotter::app::CanvasCursor;
using mandelbrotter::app::CanvasKey;
using mandelbrotter::app::MouseButton;
using mandelbrotter::app::RenderStatus;
using mandelbrotter::test::PostQueue;

/// Tiny, so a render takes microseconds even under the sanitizers.
constexpr PixelSize kSize{40, 30};

RenderSettings scene()
{
    RenderSettings settings   = mandelbrotter::app::mandelbrotDefault();
    settings.maxIterations    = 200;
    settings.autoIterations   = false;
    settings.coloring.palette = "classic";
    return settings;
}

/// A controller with every hook and callback recorded; renders post to `queue`.
struct Canvas
{
    explicit Canvas(RenderSettings initial = scene(), PixelSize size = kSize, double scale = 1.0)
      : controller(std::move(initial),
                   {.post           = queue.hook(),
                    .requestRepaint = [this] { ++repaints; },
                    .setCursor      = [this](CanvasCursor cursor) { cursors.push_back(cursor); },
                    .captureMouse   = [this](bool on) { captures.push_back(on); }})
    {
        controller.onViewChanged  = [this](const ViewSpec& view) { views.push_back(view); };
        controller.onPointerMoved = [this](const std::optional<BigComplex>& pointer) {
            pointers.push_back(pointer);
        };
        controller.onSeedPicked   = [this](Complex seed) { seeds.push_back(seed); };
        controller.onRenderStatus = [this](const RenderStatus& status) {
            statuses.push_back(status);
        };
        controller.onUserInput = [this] { ++userInputs; };
        controller.setSize(size, scale);
    }

    /// True when the last render reported has finished.
    [[nodiscard]] bool rendered() const { return !statuses.empty() && !statuses.back().rendering; }
    /// Runs the posted work until the render in progress has finished.
    [[nodiscard]] bool finishRender()
    {
        return queue.waitAndDrainUntil([this] { return rendered(); });
    }
    /// Renders the settings given at construction to completion.
    [[nodiscard]] bool renderInitial()
    {
        controller.resizeSettled();
        return finishRender();
    }
    [[nodiscard]] Viewport viewport(PixelSize device = kSize) const
    {
        return {controller.settings().view, device};
    }

    PostQueue                              queue;
    int                                    repaints{0};
    std::vector<CanvasCursor>              cursors;
    std::vector<bool>                      captures;
    std::vector<ViewSpec>                  views;
    std::vector<std::optional<BigComplex>> pointers;
    std::vector<Complex>                   seeds;
    std::vector<RenderStatus>              statuses;
    int                                    userInputs{0};
    CanvasController controller;  ///< last: destroyed first, while the queue still exists
};

RgbImage expectedImage(const RenderSettings& settings, PixelSize size)
{
    const auto buffer = mandelbrotter::renderSync(settings, size);
    EXPECT_TRUE(buffer.has_value());
    return mandelbrotter::colorized(
        *buffer, mandelbrotter::paletteOrDefault(settings.coloring.palette), settings.coloring);
}

std::vector<PixelPoint> expectedOrbit(const std::vector<Complex>& points, const Viewport& vp,
                                      double scale)
{
    std::vector<PixelPoint> out;
    for (const Complex z : points)
    {
        const PixelPoint p       = vp.toPixel(z);
        const auto       logical = [scale](int v) {
            return std::clamp(static_cast<int>(std::lround(v / scale)), -10000, 10000);
        };
        out.push_back({logical(p.x), logical(p.y)});
    }
    return out;
}

// ---------------------------------------------------------------------------------------------------------------
// Rendering

TEST(CanvasController, RenderReportsRenderingThenRendered)
{
    Canvas canvas;
    EXPECT_TRUE(canvas.controller.hasCoarsePicture());  // nothing to wait for yet
    EXPECT_FALSE(canvas.controller.rendering());
    canvas.controller.resizeSettled();
    ASSERT_EQ(canvas.statuses.size(), 1U);
    EXPECT_TRUE(canvas.statuses[0].rendering);
    EXPECT_EQ(canvas.statuses[0].iterations, 200);
    EXPECT_FALSE(canvas.controller.hasCoarsePicture());
    EXPECT_EQ(canvas.controller.image().size(), kSize);

    ASSERT_TRUE(canvas.finishRender());
    ASSERT_EQ(canvas.statuses.size(), 2U);
    EXPECT_FALSE(canvas.statuses[1].rendering);
    EXPECT_EQ(canvas.statuses[1].iterations, 200);
    EXPECT_TRUE(canvas.controller.hasCoarsePicture());
    EXPECT_EQ(canvas.controller.image(), expectedImage(scene(), kSize));
    EXPECT_GT(canvas.repaints, 0);
}

TEST(CanvasController, RenderingFollowsTheStatusItReports)
{
    Canvas            canvas;
    std::vector<bool> seen;  // rendering() as each status arrives
    canvas.controller.onRenderStatus = [&](const RenderStatus& status) {
        canvas.statuses.push_back(status);
        seen.push_back(canvas.controller.rendering());
    };
    canvas.controller.resizeSettled();
    EXPECT_TRUE(canvas.controller.rendering());
    ASSERT_TRUE(canvas.finishRender());
    // Over for whoever is told so, though the worker may still be winding down.
    EXPECT_EQ(seen, (std::vector<bool>{true, false}));
    EXPECT_FALSE(canvas.controller.rendering());
}

TEST(CanvasController, ACanceledRenderIsNoLongerRendering)
{
    Canvas canvas;
    canvas.controller.resizeSettled();
    ASSERT_TRUE(canvas.controller.rendering());
    canvas.controller.cancelRender();
    EXPECT_FALSE(canvas.controller.rendering());
    canvas.queue.drainAll();  // whatever the render had posted changes nothing
    EXPECT_FALSE(canvas.controller.rendering());
}

TEST(CanvasController, TheImageIsInDevicePixels)
{
    Canvas canvas(scene(), {20, 15}, 2.0);
    ASSERT_TRUE(canvas.renderInitial());
    EXPECT_EQ(canvas.controller.image(), expectedImage(scene(), {40, 30}));
}

TEST(CanvasController, NothingHappensUntilThePostedWorkRuns)
{
    Canvas canvas;
    canvas.controller.resizeSettled();
    const auto version = canvas.controller.imageVersion();
    ASSERT_TRUE(canvas.queue.waitForPending(1));
    EXPECT_EQ(canvas.controller.imageVersion(), version);
    EXPECT_FALSE(canvas.controller.hasCoarsePicture());
    EXPECT_TRUE(canvas.queue.drainOne());
    EXPECT_NE(canvas.controller.imageVersion(), version);
}

TEST(CanvasController, TheCoarseFlagWaitsForTheWholeFirstPass)
{
    // 150 x 100 is six 64-pixel tiles; the first pass reports all six before the second begins.
    Canvas canvas(scene(), {150, 100});
    canvas.controller.resizeSettled();
    for (int tile = 0; tile < 5; ++tile)
    {
        ASSERT_TRUE(canvas.queue.waitAndDrainOne());
        EXPECT_FALSE(canvas.controller.hasCoarsePicture()) << "after tile " << tile;
    }
    ASSERT_TRUE(canvas.queue.waitAndDrainOne());
    EXPECT_TRUE(canvas.controller.hasCoarsePicture());
    ASSERT_TRUE(canvas.finishRender());
    EXPECT_TRUE(canvas.controller.hasCoarsePicture());
}

TEST(CanvasController, TilesOfAnEarlierRenderAreDropped)
{
    Canvas         canvas;
    RenderSettings first = scene();
    first.view.zoom      = 4.0;
    canvas.controller.setSettings(first);
    ASSERT_TRUE(canvas.queue.waitForPending(1));  // at least one tile of `first` is queued

    const RenderSettings second = scene();
    canvas.controller.setSettings(second);  // cancels `first` and starts over
    ASSERT_TRUE(canvas.finishRender());
    canvas.queue.drainAll();
    EXPECT_EQ(canvas.controller.image(), expectedImage(second, kSize));
    // One finished render: the canceled one's completion is ignored.
    EXPECT_EQ(
        std::ranges::count_if(canvas.statuses, [](const RenderStatus& s) { return !s.rendering; }),
        1);
}

TEST(CanvasController, AColoringChangeRecolorsWithoutRendering)
{
    Canvas canvas;
    ASSERT_TRUE(canvas.renderInitial());
    const auto     statuses = canvas.statuses.size();
    const auto     version  = canvas.controller.imageVersion();
    RenderSettings fire     = scene();
    fire.coloring.palette   = "fire";
    fire.coloring.offset    = 0.3;
    canvas.controller.setSettings(fire);
    EXPECT_EQ(canvas.statuses.size(), statuses);
    EXPECT_FALSE(canvas.controller.rendering());
    EXPECT_EQ(canvas.queue.size(), 0U);
    EXPECT_NE(canvas.controller.imageVersion(), version);
    EXPECT_EQ(canvas.controller.image(), expectedImage(fire, kSize));
    EXPECT_EQ(canvas.controller.settings(), fire);
}

TEST(CanvasController, AGeometryChangeRenders)
{
    Canvas canvas;
    ASSERT_TRUE(canvas.renderInitial());
    RenderSettings deeper = scene();
    deeper.maxIterations  = 300;
    canvas.controller.setSettings(deeper);
    ASSERT_FALSE(canvas.rendered());
    ASSERT_TRUE(canvas.finishRender());
    EXPECT_EQ(canvas.controller.image(), expectedImage(deeper, kSize));
    EXPECT_TRUE(canvas.views.empty());  // setSettings is not a view change of the user's
}

TEST(CanvasController, ResizingRendersOnlyWhenTheSizeChanged)
{
    Canvas canvas;
    ASSERT_TRUE(canvas.renderInitial());
    const auto statuses = canvas.statuses.size();
    canvas.controller.resizeSettled();
    canvas.controller.setSize(kSize, 1.0);
    canvas.controller.resizeSettled();
    EXPECT_EQ(canvas.statuses.size(), statuses);

    canvas.controller.setSize({50, 30}, 1.0);
    EXPECT_EQ(canvas.statuses.size(), statuses);  // not before the size has settled
    canvas.controller.resizeSettled();
    ASSERT_EQ(canvas.statuses.size(), statuses + 1);
    ASSERT_TRUE(canvas.finishRender());
    EXPECT_EQ(canvas.controller.image().size(), (PixelSize{50, 30}));

    canvas.controller.setSize({50, 30}, 1.5);  // a new content scale: more device pixels
    canvas.controller.resizeSettled();
    ASSERT_TRUE(canvas.finishRender());
    EXPECT_EQ(canvas.controller.image().size(), (PixelSize{75, 45}));
}

TEST(CanvasController, PendingWorkAfterDestructionDoesNothing)
{
    PostQueue queue;
    {
        CanvasController controller(
            scene(),
            {.post = queue.hook(), .requestRepaint = {}, .setCursor = {}, .captureMouse = {}});
        controller.setSize({200, 150}, 1.0);
        controller.resizeSettled();
        ASSERT_TRUE(queue.waitForPending(2));
    }
    EXPECT_GE(queue.drainAll(), 2U);  // the closures run and find the controller gone
}

// ---------------------------------------------------------------------------------------------------------------
// Mouse

TEST(CanvasController, AWheelNotchZoomsAroundThePointer)
{
    Canvas           canvas;
    const PixelPoint at{10, 7};
    const Complex    before = canvas.viewport().pixelCenter(at);
    canvas.controller.wheel(at, 1.0);
    EXPECT_EQ(canvas.userInputs, 1);
    ASSERT_EQ(canvas.views.size(), 1U);
    EXPECT_DOUBLE_EQ(canvas.views[0].zoom, 1.25);
    EXPECT_EQ(canvas.controller.settings().view, canvas.views[0]);
    const Complex after = canvas.viewport().pixelCenter(at);
    EXPECT_NEAR(after.re, before.re, 1e-12);
    EXPECT_NEAR(after.im, before.im, 1e-12);

    canvas.controller.wheel(at, -2.0);
    EXPECT_DOUBLE_EQ(canvas.views.back().zoom, 1.25 / (1.25 * 1.25));
    EXPECT_EQ(canvas.statuses.back().rendering, true);  // every view change renders
}

TEST(CanvasController, TheWheelAnchorIsConvertedToDevicePixels)
{
    Canvas           canvas(scene(), {20, 15}, 2.0);
    const Viewport   before = canvas.viewport({40, 30});
    const PixelPoint at{5, 4};
    canvas.controller.wheel(at, 1.0);
    EXPECT_EQ(canvas.views.back(), before.zoomedAt({10, 8}, 1.25).view());
}

TEST(CanvasController, ALeftDragPans)
{
    Canvas canvas;
    ASSERT_TRUE(canvas.renderInitial());
    const RgbImage old    = canvas.controller.image();
    const Viewport before = canvas.viewport();

    canvas.controller.buttonDown(MouseButton::LEFT, {10, 10}, false);
    EXPECT_EQ(canvas.userInputs, 1);
    EXPECT_EQ(canvas.captures, std::vector<bool>{true});
    canvas.controller.pointerMoved({17, 14});
    EXPECT_EQ(canvas.controller.panOffset(), (PixelPoint{7, 4}));
    EXPECT_FALSE(canvas.controller.rubberBand().has_value());
    EXPECT_TRUE(canvas.views.empty());

    canvas.controller.buttonUp(MouseButton::LEFT, {17, 14});
    EXPECT_EQ(canvas.captures, (std::vector<bool>{true, false}));
    EXPECT_EQ(canvas.controller.panOffset(), (PixelPoint{}));
    ASSERT_EQ(canvas.views.size(), 1U);
    EXPECT_EQ(canvas.views[0], before.panned(7, 4).view());

    // Until the new render's first pass lands, the old picture stands in, shifted.
    RgbImage placeholder(kSize.width, kSize.height);
    mandelbrotter::blitShifted(old, 7, 4, placeholder);
    EXPECT_EQ(canvas.controller.image(), placeholder);
    EXPECT_FALSE(canvas.controller.hasCoarsePicture());
    ASSERT_TRUE(
        canvas.queue.waitAndDrainUntil([&] { return canvas.controller.hasCoarsePicture(); }));
    ASSERT_TRUE(canvas.finishRender());
    RenderSettings panned = scene();
    panned.view           = canvas.views[0];
    EXPECT_EQ(canvas.controller.image(), expectedImage(panned, kSize));
}

TEST(CanvasController, APanOfAnyDistanceCountsButAClickDoesNot)
{
    Canvas         canvas;
    const Viewport start = canvas.viewport();
    // The drag threshold is for rubber bands only: a 3-pixel drag still pans.
    canvas.controller.buttonDown(MouseButton::LEFT, {10, 10}, false);
    canvas.controller.buttonUp(MouseButton::LEFT, {13, 10});
    ASSERT_EQ(canvas.views.size(), 1U);
    EXPECT_EQ(canvas.views[0], start.panned(3, 0).view());

    const int repaints = canvas.repaints;
    canvas.controller.buttonDown(MouseButton::LEFT, {10, 10}, false);
    canvas.controller.buttonUp(MouseButton::LEFT, {10, 10});
    EXPECT_EQ(canvas.views.size(), 1U);
    EXPECT_GT(canvas.repaints, repaints);
}

TEST(CanvasController, AScaledPanMovesByDevicePixels)
{
    Canvas         canvas(scene(), {20, 15}, 2.0);
    const Viewport before = canvas.viewport({40, 30});
    canvas.controller.buttonDown(MouseButton::LEFT, {5, 5}, false);
    canvas.controller.buttonUp(MouseButton::LEFT, {8, 4});
    EXPECT_EQ(canvas.views.back(), before.panned(6, -2).view());
}

TEST(CanvasController, ARightClickZoomsOutAtThePointer)
{
    Canvas         canvas;
    const Viewport before = canvas.viewport();
    canvas.controller.buttonDown(MouseButton::RIGHT, {20, 15}, false);
    EXPECT_EQ(canvas.userInputs, 1);
    // Within the 3-pixel threshold: a click, zooming out at the release point.
    canvas.controller.buttonUp(MouseButton::RIGHT, {23, 12});
    ASSERT_EQ(canvas.views.size(), 1U);
    EXPECT_EQ(canvas.views[0], before.zoomedAt({23, 12}, 0.5).view());
    EXPECT_EQ(canvas.captures, (std::vector<bool>{true, false}));
}

TEST(CanvasController, ARightDragZoomsToTheRectangle)
{
    Canvas         canvas;
    const Viewport before = canvas.viewport();
    canvas.controller.buttonDown(MouseButton::RIGHT, {20, 15}, false);
    canvas.controller.pointerMoved({24, 15});
    EXPECT_EQ(canvas.controller.rubberBand(), (PixelRect{20, 15, 5, 1}));
    EXPECT_EQ(canvas.controller.panOffset(), (PixelPoint{}));
    // Four pixels is beyond the threshold: the rectangle, both corners included.
    canvas.controller.buttonUp(MouseButton::RIGHT, {24, 15});
    ASSERT_EQ(canvas.views.size(), 1U);
    EXPECT_EQ(canvas.views[0], before.zoomedToRect({20, 15, 5, 1}).view());
    EXPECT_FALSE(canvas.controller.rubberBand().has_value());
}

TEST(CanvasController, AShiftDragZoomsToTheRectangleInAnyDirection)
{
    Canvas         canvas;
    const Viewport before = canvas.viewport();
    canvas.controller.buttonDown(MouseButton::LEFT, {30, 20}, true);
    canvas.controller.pointerMoved({10, 5});
    EXPECT_EQ(canvas.controller.rubberBand(), (PixelRect{10, 5, 21, 16}));
    canvas.controller.buttonUp(MouseButton::LEFT, {10, 5});
    EXPECT_EQ(canvas.views.back(), before.zoomedToRect({10, 5, 21, 16}).view());
}

TEST(CanvasController, AScaledRubberBandCoversWholeLogicalPixels)
{
    Canvas         canvas(scene(), {20, 15}, 2.0);
    const Viewport before = canvas.viewport({40, 30});
    canvas.controller.buttonDown(MouseButton::RIGHT, {2, 3}, false);
    canvas.controller.buttonUp(MouseButton::RIGHT, {12, 9});
    // Logical (2, 3)..(12, 9) inclusive is device (4, 6)..(26, 20) exclusive.
    EXPECT_EQ(canvas.views.back(), before.zoomedToRect({4, 6, 22, 14}).view());
}

TEST(CanvasController, ALeftClickWithShiftUnderTheThresholdChangesNothing)
{
    Canvas canvas;
    canvas.controller.buttonDown(MouseButton::LEFT, {10, 10}, true);
    canvas.controller.buttonUp(MouseButton::LEFT, {12, 13});
    EXPECT_TRUE(canvas.views.empty());
}

TEST(CanvasController, ReleasingWithoutADragDoesNothing)
{
    Canvas canvas;
    canvas.controller.buttonUp(MouseButton::LEFT, {10, 10});
    canvas.controller.buttonUp(MouseButton::RIGHT, {10, 10});
    EXPECT_TRUE(canvas.views.empty());
    EXPECT_TRUE(canvas.captures.empty());
}

TEST(CanvasController, LosingTheCaptureAbandonsTheDrag)
{
    Canvas canvas;
    canvas.controller.buttonDown(MouseButton::LEFT, {10, 10}, false);
    canvas.controller.pointerMoved({20, 20});
    ASSERT_EQ(canvas.controller.panOffset(), (PixelPoint{10, 10}));
    canvas.controller.captureLost();
    EXPECT_EQ(canvas.controller.panOffset(), (PixelPoint{}));
    canvas.controller.buttonUp(MouseButton::LEFT, {20, 20});
    EXPECT_TRUE(canvas.views.empty());
    EXPECT_EQ(canvas.captures, std::vector<bool>{true});  // nothing left to release
}

TEST(CanvasController, PickModeReportsTheSeedInsteadOfDragging)
{
    Canvas canvas;
    canvas.controller.setPickSeedMode(true);
    EXPECT_TRUE(canvas.controller.pickSeedMode());
    EXPECT_EQ(canvas.cursors, std::vector<CanvasCursor>{CanvasCursor::BULLSEYE});

    canvas.controller.buttonDown(MouseButton::LEFT, {10, 7}, false);
    EXPECT_EQ(canvas.userInputs, 1);
    ASSERT_EQ(canvas.seeds.size(), 1U);
    EXPECT_EQ(canvas.seeds[0], canvas.viewport().pixelCenter({10, 7}));
    EXPECT_TRUE(canvas.captures.empty());
    canvas.controller.buttonUp(MouseButton::LEFT, {10, 7});
    EXPECT_TRUE(canvas.views.empty());

    canvas.controller.setPickSeedMode(false);
    EXPECT_EQ(canvas.cursors,
              (std::vector<CanvasCursor>{CanvasCursor::BULLSEYE, CanvasCursor::CROSS}));
}

TEST(CanvasController, PickModeLeavesTheRightButtonAlone)
{
    Canvas canvas;
    canvas.controller.setPickSeedMode(true);
    canvas.controller.buttonDown(MouseButton::RIGHT, {10, 7}, false);
    EXPECT_TRUE(canvas.seeds.empty());
    EXPECT_TRUE(canvas.controller.rubberBand().has_value());
}

TEST(CanvasController, ThePointerIsReportedAtFullPrecision)
{
    Canvas canvas;
    canvas.controller.pointerMoved({10, 7});
    ASSERT_EQ(canvas.pointers.size(), 1U);
    ASSERT_TRUE(canvas.pointers[0].has_value());
    EXPECT_EQ(*canvas.pointers[0], canvas.viewport().pixelCenterBig({10, 7}));
    EXPECT_TRUE(canvas.controller.orbit().empty());  // the overlay is off

    canvas.controller.pointerLeft();
    ASSERT_EQ(canvas.pointers.size(), 2U);
    EXPECT_FALSE(canvas.pointers[1].has_value());
}

// ---------------------------------------------------------------------------------------------------------------
// Orbit overlay

TEST(CanvasController, TheOrbitFollowsThePointer)
{
    Canvas canvas(scene(), {20, 15}, 2.0);
    canvas.controller.setShowOrbit(true);
    EXPECT_TRUE(canvas.controller.showOrbit());
    const PixelPoint at{3, 2};  // near the corner: the orbit escapes far beyond the window
    canvas.controller.pointerMoved(at);
    const Viewport vp     = canvas.viewport({40, 30});
    const auto     points = mandelbrotter::orbit(scene().fractal, vp.pixelCenter({6, 4}), 200);
    const std::vector<PixelPoint> expected = expectedOrbit(points, vp, 2.0);
    const auto                    orbit    = canvas.controller.orbit();
    EXPECT_EQ(std::vector<PixelPoint>(orbit.begin(), orbit.end()), expected);
    EXPECT_GT(orbit.size(), 1U);
    EXPECT_TRUE(std::ranges::any_of(
        orbit, [](PixelPoint p) { return std::abs(p.x) == 10000 || std::abs(p.y) == 10000; }));

    canvas.controller.buttonDown(MouseButton::LEFT, {5, 5}, false);
    canvas.controller.pointerMoved({6, 6});  // no new orbit while dragging
    const auto during = canvas.controller.orbit();
    EXPECT_EQ(std::vector<PixelPoint>(during.begin(), during.end()), expected);
}

TEST(CanvasController, TheOrbitIsCappedAt512Points)
{
    RenderSettings settings = scene();
    settings.maxIterations  = 5000;
    Canvas canvas(settings);
    canvas.controller.setShowOrbit(true);
    canvas.controller.pointerMoved({20, 15});  // the center, -0.5: inside the set
    EXPECT_EQ(canvas.controller.orbit().size(), 512U);
}

TEST(CanvasController, TheDeepOrbitIsExact)
{
    RenderSettings settings = mandelbrotter::app::deepSeahorse(1e10);
    settings.autoIterations = false;
    settings.maxIterations  = 300;
    Canvas canvas(settings);
    canvas.controller.setShowOrbit(true);
    canvas.controller.pointerMoved({12, 9});
    const Viewport                      vp = canvas.viewport();
    const mandelbrotter::ReferenceOrbit exact(settings.fractal, vp.pixelCenterBig({12, 9}), 299);
    const std::vector<Complex>          points(exact.points().begin(), exact.points().end());
    const auto                          orbit = canvas.controller.orbit();
    EXPECT_EQ(std::vector<PixelPoint>(orbit.begin(), orbit.end()), expectedOrbit(points, vp, 1.0));
}

TEST(CanvasController, ADeepJuliaOrbitStartsAtThePointer)
{
    RenderSettings settings = mandelbrotter::app::deepSeahorse(1e10);
    settings.fractal.julia  = true;
    settings.fractal.seed   = mandelbrotter::app::kJuliaSeed;
    Canvas canvas(settings);
    canvas.controller.setShowOrbit(true);
    canvas.controller.pointerMoved({12, 9});
    ASSERT_FALSE(canvas.controller.orbit().empty());
    EXPECT_EQ(canvas.controller.orbit().front(), (PixelPoint{12, 9}));
}

TEST(CanvasController, LeavingTheWindowClearsTheOrbit)
{
    Canvas canvas;
    canvas.controller.setShowOrbit(true);
    canvas.controller.pointerMoved({10, 7});
    ASSERT_FALSE(canvas.controller.orbit().empty());
    canvas.controller.pointerLeft();
    EXPECT_TRUE(canvas.controller.orbit().empty());

    canvas.controller.pointerMoved({10, 7});
    canvas.controller.setShowOrbit(false);
    EXPECT_TRUE(canvas.controller.orbit().empty());
}

TEST(CanvasController, APinnedOrbitStaysUntilThePointerMoves)
{
    Canvas        canvas;
    const Complex point = mandelbrotter::app::kOrbitPoint;
    canvas.controller.showOrbitAt(point);
    ASSERT_EQ(canvas.pointers.size(), 1U);
    EXPECT_EQ(canvas.pointers[0],
              BigComplex::fromComplex(point, mandelbrotter::fractionBitsFor(1.0)));
    const std::vector<PixelPoint> pinned(canvas.controller.orbit().begin(),
                                         canvas.controller.orbit().end());
    EXPECT_GT(pinned.size(), 1U);                            // computed even with the overlay off
    EXPECT_EQ(pinned[1], canvas.viewport().toPixel(point));  // z0 = 0, z1 = c

    canvas.controller.pointerLeft();  // the mouse left the window: the pin holds
    EXPECT_EQ(canvas.pointers.size(), 1U);
    EXPECT_EQ(canvas.controller.orbit().size(), pinned.size());

    canvas.controller.clearPinnedOrbit();
    EXPECT_TRUE(canvas.controller.orbit().empty());
    ASSERT_EQ(canvas.pointers.size(), 2U);
    EXPECT_FALSE(canvas.pointers[1].has_value());
    canvas.controller.clearPinnedOrbit();  // nothing pinned any more
    EXPECT_EQ(canvas.pointers.size(), 2U);
}

TEST(CanvasController, MovingThePointerUnpins)
{
    Canvas canvas;
    canvas.controller.showOrbitAt(mandelbrotter::app::kOrbitPoint);
    canvas.controller.pointerMoved({1, 1});
    canvas.controller.pointerLeft();
    EXPECT_TRUE(canvas.controller.orbit().empty());
    EXPECT_FALSE(canvas.pointers.back().has_value());
}

// ---------------------------------------------------------------------------------------------------------------
// Keyboard and commands

TEST(CanvasController, ArrowKeysPanByATenth)
{
    Canvas         canvas;
    const Viewport start = canvas.viewport();
    // 40 x 30 pixels: 4 across, 3 down.
    EXPECT_TRUE(canvas.controller.key(CanvasKey::LEFT));
    EXPECT_EQ(canvas.views.back(), start.panned(4, 0).view());
    canvas.controller.key(CanvasKey::RIGHT);
    canvas.controller.key(CanvasKey::RIGHT);
    EXPECT_EQ(canvas.views.back(), start.panned(4, 0).panned(-4, 0).panned(-4, 0).view());
    canvas.controller.key(CanvasKey::UP);
    const Viewport afterUp = canvas.viewport();
    canvas.controller.key(CanvasKey::DOWN);
    EXPECT_EQ(canvas.views.back(), afterUp.panned(0, -3).view());
    EXPECT_EQ(canvas.views.size(), 5U);
    EXPECT_EQ(canvas.userInputs, 0);  // keys are not mouse input
}

TEST(CanvasController, ZoomKeysZoomAtTheCenter)
{
    Canvas         canvas;
    const Viewport start = canvas.viewport();
    EXPECT_TRUE(canvas.controller.key(CanvasKey::ZOOM_IN));
    EXPECT_EQ(canvas.views.back(), start.zoomedAt({20, 15}, 2.0).view());
    const Viewport zoomed = canvas.viewport();
    EXPECT_TRUE(canvas.controller.key(CanvasKey::ZOOM_OUT));
    EXPECT_EQ(canvas.views.back(), zoomed.zoomedAt({20, 15}, 0.5).view());

    canvas.controller.zoomAtCenter(8.0);
    EXPECT_DOUBLE_EQ(canvas.views.back().zoom, 8.0);
}

TEST(CanvasController, HomeResetsTheView)
{
    RenderSettings settings = mandelbrotter::app::seahorse();
    settings.fractal.family = mandelbrotter::FractalFamily::TRICORN;
    Canvas canvas(settings);
    EXPECT_TRUE(canvas.controller.key(CanvasKey::HOME));
    EXPECT_EQ(canvas.views.back(), mandelbrotter::defaultView(settings.fractal));
    canvas.controller.key(CanvasKey::ZOOM_IN);
    canvas.controller.resetView();
    EXPECT_EQ(canvas.views.back(), mandelbrotter::defaultView(settings.fractal));
}

TEST(CanvasController, EscapeAbandonsADragAndIsAlwaysHandled)
{
    Canvas canvas;
    EXPECT_TRUE(canvas.controller.key(CanvasKey::ESCAPE));
    EXPECT_TRUE(canvas.captures.empty());
    EXPECT_TRUE(canvas.views.empty());

    canvas.controller.buttonDown(MouseButton::RIGHT, {10, 10}, false);
    canvas.controller.pointerMoved({30, 25});
    ASSERT_TRUE(canvas.controller.rubberBand().has_value());
    EXPECT_TRUE(canvas.controller.key(CanvasKey::ESCAPE));
    EXPECT_FALSE(canvas.controller.rubberBand().has_value());
    EXPECT_EQ(canvas.captures, (std::vector<bool>{true, false}));
    canvas.controller.buttonUp(MouseButton::RIGHT, {30, 25});
    EXPECT_TRUE(canvas.views.empty());

    canvas.controller.buttonDown(MouseButton::LEFT, {10, 10}, false);
    canvas.controller.pointerMoved({30, 25});
    canvas.controller.key(CanvasKey::ESCAPE);
    EXPECT_EQ(canvas.controller.panOffset(), (PixelPoint{}));
}

TEST(CanvasController, TheHighlightRepaintsOnlyWhenItChanges)
{
    Canvas canvas;
    canvas.controller.setHighlighted(true);
    EXPECT_TRUE(canvas.controller.highlighted());
    const int repaints = canvas.repaints;
    canvas.controller.setHighlighted(true);
    EXPECT_EQ(canvas.repaints, repaints);
    canvas.controller.setHighlighted(false);
    EXPECT_FALSE(canvas.controller.highlighted());
    EXPECT_EQ(canvas.repaints, repaints + 1);
}

TEST(CanvasController, HooksAreOptional)
{
    CanvasController controller(scene(), {});
    controller.setSize(kSize, 1.0);
    controller.setPickSeedMode(true);
    controller.setShowOrbit(true);
    controller.pointerMoved({3, 3});
    controller.buttonDown(MouseButton::RIGHT, {1, 1}, false);
    controller.buttonUp(MouseButton::RIGHT, {30, 20});
    controller.key(CanvasKey::HOME);
    EXPECT_FALSE(controller.orbit().empty());
    controller.cancelRender();
    EXPECT_FALSE(controller.rendering());
}

}  // namespace
