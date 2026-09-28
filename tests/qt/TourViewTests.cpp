#include "qt/TourView.h"

#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRect>
#include <QTest>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/TourScript.h"
#include "Mandelbrotter/app/panel_model.h"
#include "Mandelbrotter/app/scenes.h"
#include "Mandelbrotter/app/ui_text.h"
#include "QtHarness.h"
#include "qt/FractalCanvas.h"
#include "qt/HelpWindow.h"
#include "qt/MainWindow.h"
#include "qt/SidePanel.h"
#include "qt/TourCard.h"
#include "qt/qt_util.h"

namespace
{

using mandelbrotter::app::PanelSection;
using mandelbrotter::test::pumpUntil;
using mandelbrotter::test::QtHarness;
namespace app = mandelbrotter::app;
namespace qt  = mandelbrotter::qt;

/// A main window showing a view of its own, with a bookmark and the orbit overlay, and the tour
/// started on it.
struct TourHarness : QtHarness
{
    TourHarness() : QtHarness(app::seahorseFire())
    {
        activate();
        window.app().addBookmark("mine");
        window.app().setShowOrbit(true);
        window.app().startTour();
    }

    [[nodiscard]] app::TourScript& tour() { return window.app().tour(); }
    /// The card, which must be shown.
    [[nodiscard]] qt::TourCard& card()
    {
        qt::TourCard* shown = window.tourView().card();
        if (shown == nullptr)
        {
            ADD_FAILURE() << "the tour card is not shown";
            std::abort();
        }
        return *shown;
    }
    /// Lets the deferred placement and deletion run.
    static void settle() { QTest::qWait(20); }
};

TEST(TourView, WalksForwardAndBackThroughElevenSteps)
{
    TourHarness h;
    ASSERT_NE(h.window.tourView().card(), nullptr);
    EXPECT_TRUE(h.card().isVisible());
    EXPECT_EQ(h.card().titleLabel().text(), qt::toQt(h.tour().steps()[0].title));
    EXPECT_EQ(h.card().progressLabel().text(), qt::toQt(app::stepText(0, 11)));
    EXPECT_FALSE(h.card().backButton().isEnabled());
    EXPECT_EQ(h.card().nextButton().text(), qt::toQt(app::kNext));
    for (std::size_t i = 1; i < 11; ++i)
    {
        h.card().nextButton().click();
        EXPECT_EQ(h.tour().currentStep(), i);
        EXPECT_EQ(h.card().titleLabel().text(), qt::toQt(h.tour().steps()[i].title));
        EXPECT_TRUE(h.card().backButton().isEnabled());
    }
    EXPECT_EQ(h.card().nextButton().text(), qt::toQt(app::kFinish));
    EXPECT_EQ(h.card().progressLabel().text(), qt::toQt(app::stepText(10, 11)));
    h.card().backButton().click();
    EXPECT_EQ(h.tour().currentStep(), 9U);
}

TEST(TourView, TheCardFitsEachStepsText)
{
    TourHarness h;
    for (std::size_t step = 0; step < h.tour().stepCount(); ++step)
    {
        SCOPED_TRACE(step);
        h.tour().showStep(step);
        TourHarness::settle();
        const QLabel& text = h.card().textLabel();
        const int     needed =
            text.fontMetrics()
                .boundingRect(QRect(0, 0, text.width(), 10000), Qt::TextWordWrap, text.text())
                .height();
        // All of it shows, and no longer step leaves its height behind.
        EXPECT_GE(text.height(), needed);
        EXPECT_LE(text.height(), needed + text.fontMetrics().lineSpacing());
    }
}

TEST(TourView, TheCardSitsInsideTheWindowBesideItsAnchor)
{
    TourHarness h;
    for (const std::size_t step : {0U, 2U, 4U, 7U})
    {
        SCOPED_TRACE(step);
        h.tour().showStep(step);
        TourHarness::settle();
        const QRect card   = h.card().geometry();
        const QRect anchor = h.window.tourView().anchorRect();
        EXPECT_TRUE(h.window.rect().contains(card));
        if (app::panelSectionFor(h.tour().steps()[step].anchor))
        {
            // Beside the section: to its left, where the canvas has room, top-aligned unless the
            // window's bottom pushes it up.
            EXPECT_LE(card.right(), anchor.left());
            EXPECT_LE(card.top(), anchor.top());
            EXPECT_TRUE(card.top() == anchor.top() || card.bottom() == h.window.rect().bottom());
        }
        else
        {
            EXPECT_TRUE(anchor.contains(card.topLeft()));
        }
    }
    h.tour().showStep(2);
    h.window.resize(h.window.width() - 150, h.window.height() - 80);
    ASSERT_TRUE(pumpUntil([&h] {
        return h.window.tourView().anchorRect().top() == h.card().geometry().top() &&
               h.card().geometry().right() <= h.window.tourView().anchorRect().left();
    }));
    EXPECT_TRUE(h.window.rect().contains(h.card().geometry()));
}

TEST(TourView, RingsTheCanvasOrASection)
{
    TourHarness h;
    const auto& canvas = h.window.canvas().controller();
    EXPECT_TRUE(canvas.highlighted());
    EXPECT_EQ(h.window.panel().highlightedSection(), std::nullopt);
    h.tour().showStep(4);  // Iterations
    EXPECT_FALSE(canvas.highlighted());
    EXPECT_EQ(h.window.panel().highlightedSection(), PanelSection::ITERATIONS);
    h.tour().showStep(10);  // the last: no ring
    EXPECT_FALSE(canvas.highlighted());
    EXPECT_EQ(h.window.panel().highlightedSection(), std::nullopt);
}

TEST(TourView, APanelStepShowsAHiddenPanel)
{
    TourHarness h;
    h.window.setSidePanelShown(false);
    h.tour().showStep(6);  // the orbit overlay
    EXPECT_TRUE(h.window.panelDock().isVisible());
    EXPECT_EQ(h.window.panel().highlightedSection(), PanelSection::OVERLAY);
}

TEST(TourView, TheExportStepClosesItsDialogOnLeaving)
{
    TourHarness h;
    h.tour().showStep(8);  // Saving a picture
    EXPECT_NE(h.window.exportDialog(), nullptr);
    h.card().nextButton().click();
    EXPECT_EQ(h.window.exportDialog(), nullptr);
}

TEST(TourView, EscapeCloseAndFinishPutBackWhatTheUserHad)
{
    for (int ending = 0; ending < 3; ++ending)
    {
        SCOPED_TRACE(ending);
        TourHarness h;
        h.tour().showStep(7);  // the bookmarks step adds its example
        EXPECT_EQ(h.window.panel().controls().bookmarks->count(), 2);
        switch (ending)
        {
            case 0:
                ASSERT_NE(QApplication::focusWidget(), nullptr);
                QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
                break;
            case 1:
                h.card().closeButton().click();
                break;
            default:
                h.tour().showStep(10);
                h.card().nextButton().click();  // Finish
                break;
        }
        EXPECT_FALSE(h.window.app().tourRunning());
        EXPECT_EQ(h.window.app().settings(), app::seahorseFire());
        EXPECT_TRUE(h.window.app().showOrbit());
        EXPECT_EQ(h.window.panel().controls().bookmarks->count(), 1);
        EXPECT_FALSE(h.window.canvas().controller().highlighted());
        TourHarness::settle();
        EXPECT_EQ(h.window.tourView().card(), nullptr);
    }
}

TEST(TourView, LearnMoreOpensTheStepsPage)
{
    TourHarness h;
    h.tour().showStep(3);
    ASSERT_TRUE(h.card().learnMoreButton().isVisible());
    h.card().learnMoreButton().click();
    EXPECT_TRUE(h.window.helpShown());
    EXPECT_EQ(h.window.help().currentPage(), h.tour().steps()[3].helpPage);
}

}  // namespace
