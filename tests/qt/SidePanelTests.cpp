#include "qt/SidePanel.h"

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QLineEdit>
#include <QListWidget>
#include <QPoint>
#include <QRect>
#include <QScrollBar>
#include <QSpinBox>
#include <QTest>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/format.h"
#include "Mandelbrotter/app/panel_model.h"
#include "Mandelbrotter/app/scenes.h"
#include "Mandelbrotter/fractal.h"
#include "QtHarness.h"
#include "qt/FractalCanvas.h"
#include "qt/JuliaPreview.h"
#include "qt/qt_util.h"

namespace
{

using namespace Qt::StringLiterals;
using mandelbrotter::Complex;
using mandelbrotter::FractalFamily;
using mandelbrotter::RenderSettings;
using mandelbrotter::app::PanelSection;
using mandelbrotter::test::pumpUntil;
using mandelbrotter::test::QtHarness;
namespace app = mandelbrotter::app;
namespace qt  = mandelbrotter::qt;

/// A main window with the panel's controls at hand.
struct PanelHarness : QtHarness
{
    explicit PanelHarness(const RenderSettings& initial = app::seahorse()) : QtHarness(initial)
    {
        activate();
    }

    [[nodiscard]] qt::SidePanel&                 panel() const { return window.panel(); }
    [[nodiscard]] const qt::SidePanel::Controls& c() const { return panel().controls(); }
    [[nodiscard]] const RenderSettings&          settings() { return window.app().settings(); }

    /// Types `text` into a field and presses Enter.
    static void enter(QWidget& field, const QString& text)
    {
        field.setFocus();
        if (auto* edit = qobject_cast<QLineEdit*>(&field); edit != nullptr)
        {
            edit->selectAll();
        }
        QTest::keyClicks(&field, text);
        QTest::keyClick(&field, Qt::Key_Return);
    }
};

TEST(SidePanel, AFamilyChangeResetsTheViewAndAnExponentChangeKeepsIt)
{
    PanelHarness h;
    const auto   view = h.settings().view;
    h.c().exponent->setFocus();
    h.c().exponent->stepUp();  // the arrows commit at once
    EXPECT_EQ(h.settings().fractal.exponent, 3);
    EXPECT_EQ(h.settings().view, view);

    h.c().family->setCurrentIndex(1);
    Q_EMIT h.c().family->activated(1);  // what a user's choice emits
    EXPECT_EQ(h.settings().fractal.family, FractalFamily::BURNING_SHIP);
    EXPECT_EQ(h.settings().view, mandelbrotter::defaultView(h.settings().fractal));
}

TEST(SidePanel, PushingSettingsEchoesNothing)
{
    const PanelHarness h;
    int                edits    = 0;
    h.panel().onSettingsChanged = [&edits](const RenderSettings&) { ++edits; };
    h.panel().setSettings(app::juliaExample());
    h.panel().setSettings(app::seahorseFire());
    EXPECT_EQ(edits, 0);
    EXPECT_EQ(h.c().palette->currentText(), u"fire"_s);
    EXPECT_DOUBLE_EQ(h.c().density->value(), app::seahorseFire().coloring.density);
    EXPECT_EQ(h.c().offset->value(), app::offsetToSlider(app::seahorseFire().coloring.offset));
}

TEST(SidePanel, ABadSeedIsRestoredAndAGoodOneApplied)
{
    PanelHarness h(app::juliaExample());
    PanelHarness::enter(*h.c().seedRe, u"nonsense"_s);
    EXPECT_EQ(h.c().seedRe->text(), qt::toQt(app::formatSeedComponent(app::kJuliaSeed.re)));
    EXPECT_EQ(h.settings().fractal.seed, app::kJuliaSeed);

    PanelHarness::enter(*h.c().seedRe, u" -0.4 "_s);
    EXPECT_EQ(h.settings().fractal.seed, (Complex{-0.4, app::kJuliaSeed.im}));
    PanelHarness::enter(*h.c().seedIm, u"0.6"_s);
    EXPECT_EQ(h.settings().fractal.seed, (Complex{-0.4, 0.6}));
}

TEST(SidePanel, SpinBoxesCommitOnEnterOnly)
{
    PanelHarness h;
    const int    before = h.settings().maxIterations;
    h.c().iterations->setFocus();
    h.c().iterations->selectAll();
    QTest::keyClicks(h.c().iterations, u"777"_s);
    EXPECT_EQ(h.settings().maxIterations, before);  // typing alone changes nothing
    QTest::keyClick(h.c().iterations, Qt::Key_Return);
    EXPECT_EQ(h.settings().maxIterations, 777);
}

TEST(SidePanel, TheOffsetSliderMapsItsSteps)
{
    PanelHarness h;
    h.c().offset->setValue(250);  // a user's drag emits the same signal
    EXPECT_DOUBLE_EQ(h.settings().coloring.offset, app::sliderToOffset(250));
}

TEST(SidePanel, BookmarksAddLoadAndDelete)
{
    PanelHarness h;
    h.window.app().addBookmark("first");
    h.window.app().applySettings(app::juliaExample());
    h.window.app().addBookmark("second");
    QListWidget& list = *h.c().bookmarks;
    ASSERT_EQ(list.count(), 2);
    EXPECT_EQ(list.item(1)->text(), u"second"_s);
    EXPECT_FALSE(h.c().loadBookmark->isEnabled());
    EXPECT_FALSE(h.c().deleteBookmark->isEnabled());

    list.setCurrentRow(0);
    EXPECT_TRUE(h.c().loadBookmark->isEnabled());
    h.c().loadBookmark->click();
    EXPECT_EQ(h.settings(), app::seahorse());

    // Adding keeps the selection; activating (double-click, Enter) loads.
    h.window.app().addBookmark("third");
    EXPECT_EQ(list.currentRow(), 0);
    Q_EMIT list.itemActivated(list.item(1));
    EXPECT_EQ(h.settings(), app::juliaExample());

    list.setCurrentRow(2);
    h.c().deleteBookmark->click();
    ASSERT_EQ(list.count(), 2);
    EXPECT_EQ(h.window.app().bookmarks().bookmarks().size(), 2U);
    EXPECT_FALSE(h.c().deleteBookmark->isEnabled());  // the selected row is gone
}

TEST(SidePanel, PickSeedModeFollowsTheButtonAndTheApp)
{
    PanelHarness h;
    h.c().pickSeed->click();
    EXPECT_TRUE(h.window.canvas().controller().pickSeedMode());
    h.window.app().setPickSeedMode(false);
    EXPECT_FALSE(h.c().pickSeed->isChecked());
}

TEST(SidePanel, SectionsOfControls)
{
    const PanelHarness h;
    EXPECT_EQ(h.panel().sectionOf(h.c().seedIm), PanelSection::FRACTAL);
    EXPECT_EQ(h.panel().sectionOf(h.c().iterations), PanelSection::ITERATIONS);
    EXPECT_EQ(h.panel().sectionOf(h.c().density), PanelSection::COLORING);
    EXPECT_EQ(h.panel().sectionOf(h.c().orbit), PanelSection::OVERLAY);
    EXPECT_EQ(h.panel().sectionOf(h.c().bookmarks), PanelSection::BOOKMARKS);
    EXPECT_EQ(h.panel().sectionOf(&h.panel()), std::nullopt);
    EXPECT_TRUE(h.panel().isJuliaControl(h.c().seedRe));
    EXPECT_TRUE(h.panel().isJuliaControl(h.c().preview));
    EXPECT_FALSE(h.panel().isJuliaControl(h.c().exponent));
}

TEST(SidePanel, ScrollingBringsASectionIntoView)
{
    PanelHarness h;
    h.window.resize(h.window.width(), 500);  // the panel must scroll
    ASSERT_TRUE(pumpUntil([&] { return h.panel().verticalScrollBar()->maximum() > 0; }));
    h.panel().scrollToSection(PanelSection::BOOKMARKS);
    const QRect box      = h.panel().sectionRect(PanelSection::BOOKMARKS, *h.panel().viewport());
    const QRect viewport = h.panel().viewport()->rect();
    EXPECT_GE(box.top(), 0);
    EXPECT_TRUE(viewport.intersects(box));
    const int scrolled = h.panel().verticalScrollBar()->value();
    EXPECT_GT(scrolled, 0);
    h.panel().scrollToSection(PanelSection::FRACTAL);
    EXPECT_EQ(h.panel().verticalScrollBar()->value(), 0);
}

TEST(SidePanel, TheTourRingIsPaintedAroundTheSection)
{
    const PanelHarness h;
    h.panel().setHighlightedSection(PanelSection::OVERLAY);
    QApplication::processEvents();
    const QImage shown = h.panel().grab().toImage();
    const QRect  box   = h.panel().sectionRect(PanelSection::OVERLAY, h.panel());
    const QPoint left(box.left() - app::kSectionRingInflate, box.center().y());
    const QColor highlight = h.panel().palette().color(QPalette::Highlight);
    const auto   near      = [&](QColor pixel) {
        return std::abs(pixel.red() - highlight.red()) < 40 &&
               std::abs(pixel.green() - highlight.green()) < 40 &&
               std::abs(pixel.blue() - highlight.blue()) < 40;
    };
    EXPECT_TRUE(near(shown.pixelColor(left))) << shown.pixelColor(left).name().toStdString();
    h.panel().setHighlightedSection(std::nullopt);
    QApplication::processEvents();
    EXPECT_FALSE(near(h.panel().grab().toImage().pixelColor(left)));
}

TEST(SidePanel, ThePreviewFollowsTheHoveredPoint)
{
    PanelHarness h(app::mandelbrotDefault());
    h.window.app().setPreviewSeed(Complex{-0.1, 0.65});
    EXPECT_EQ(h.c().preview->seed(), (Complex{-0.1, 0.65}));
    ASSERT_TRUE(pumpUntil([&] { return !h.c().preview->picture().isNull(); }));
    h.window.app().setPreviewSeed(std::nullopt);
    ASSERT_TRUE(pumpUntil([&] { return h.c().preview->picture().isNull(); }));
}

}  // namespace
