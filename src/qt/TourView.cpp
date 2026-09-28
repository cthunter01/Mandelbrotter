#include "qt/TourView.h"

#include <QEvent>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QTimer>
#include <cstddef>
#include <optional>

#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/TourScript.h"
#include "qt/FractalCanvas.h"
#include "qt/MainWindow.h"
#include "qt/SidePanel.h"
#include "qt/TourCard.h"

namespace mandelbrotter::qt
{

TourView::TourView(MainWindow& window) : m_window(window) { }

TourView::~TourView()
{
    m_window.removeEventFilter(this);
    delete m_card.data();
}

void TourView::showCard(const app::TourStep& step, std::size_t index, std::size_t count)
{
    if (!m_card)
    {
        auto* card        = new TourCard(&m_window);
        card->onBack      = [this] { m_window.app().tour().back(); };
        card->onNext      = [this] { m_window.app().tour().next(); };
        card->onClose     = [this] { m_window.app().tour().stop(); };
        card->onLearnMore = [this] { learnMore(); };
        m_card            = card;
        m_window.installEventFilter(this);
    }
    m_anchor = step.anchor;
    m_card->setStep(step.title, step.text, index, count, !step.helpPage.empty());
    place();
    // Qt lays out lazily: the dock or the panel's scroll position may still move.
    QTimer::singleShot(0, this, [this] { place(); });
}

void TourView::hideCard()
{
    m_anchor.reset();
    m_window.removeEventFilter(this);
    if (TourCard* card = m_card.data(); card != nullptr)
    {
        // Close and Finish run inside the card's own button signal.
        m_card = nullptr;
        card->hide();
        card->deleteLater();
    }
}

void TourView::highlight(std::optional<app::TourTarget> target)
{
    m_window.canvas().controller().setHighlighted(target == app::TourTarget::CANVAS);
    const std::optional<app::PanelSection> section =
        target ? app::panelSectionFor(*target) : std::nullopt;
    if (section)
    {
        m_window.setSidePanelShown(true);
        m_window.panel().scrollToSection(*section);
    }
    m_window.panel().setHighlightedSection(section);
}

QRect TourView::anchorRect() const
{
    if (!m_anchor)
    {
        return {};
    }
    if (const auto section = app::panelSectionFor(*m_anchor))
    {
        return m_window.panel().sectionRect(*section, m_window);
    }
    const FractalCanvas& canvas = m_window.canvas();
    return {canvas.mapTo(&m_window, QPoint(0, 0)), canvas.size()};
}

void TourView::place()
{
    if (m_card && m_anchor)
    {
        m_card->placeNear(anchorRect());
    }
}

void TourView::learnMore()
{
    const app::TourScript& tour = m_window.app().tour();
    if (tour.currentStep() < tour.stepCount())
    {
        m_window.app().showHelpPage(tour.steps()[tour.currentStep()].helpPage);
    }
}

bool TourView::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == &m_window && event->type() == QEvent::Resize && m_card)
    {
        // The layout moves the canvas and the panel after this event; place the card once it has.
        QTimer::singleShot(0, this, [this] { place(); });
    }
    return false;
}

}  // namespace mandelbrotter::qt
