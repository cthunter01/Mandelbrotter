#include "gui/TourView.h"

#include <cstddef>
#include <optional>

#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/TourScript.h"
#include "gui/FractalCanvas.h"
#include "gui/MainFrame.h"
#include "gui/SidePanel.h"
#include "gui/TourCard.h"

namespace mandelbrotter::gui
{

namespace
{

/// The side panel section a tour target is; nullopt for the canvas.
std::optional<SidePanel::Section> sectionFor(app::TourTarget target)
{
    switch (target)
    {
        case app::TourTarget::FRACTAL:
            return SidePanel::Section::FRACTAL;
        case app::TourTarget::ITERATIONS:
            return SidePanel::Section::ITERATIONS;
        case app::TourTarget::COLOURING:
            return SidePanel::Section::COLOURING;
        case app::TourTarget::OVERLAY:
            return SidePanel::Section::OVERLAY;
        case app::TourTarget::BOOKMARKS:
            return SidePanel::Section::BOOKMARKS;
        case app::TourTarget::CANVAS:
            break;
    }
    return std::nullopt;
}

}  // namespace

TourView::TourView(MainFrame& frame) : m_frame(frame) { }

TourView::~TourView() = default;

void TourView::showCard(const app::TourStep& step, std::size_t index, std::size_t count)
{
    if (m_card == nullptr)
    {
        m_card              = new TourCard(&m_frame);
        m_card->onBack      = [this] { m_frame.app().tour().back(); };
        m_card->onNext      = [this] { m_frame.app().tour().next(); };
        m_card->onClose     = [this] { m_frame.app().tour().stop(); };
        m_card->onLearnMore = [this] { learnMore(); };
        m_frame.Bind(wxEVT_SIZE, &TourView::onFrameResized, this);
    }
    m_anchor = step.anchor;
    m_card->setStep(step.title, step.text, index, count, !step.helpPage.empty());
    m_card->placeNear(anchorFor(step.anchor));
}

void TourView::hideCard()
{
    m_anchor.reset();
    if (m_card == nullptr)
    {
        return;
    }
    m_frame.Unbind(wxEVT_SIZE, &TourView::onFrameResized, this);
    m_card->Destroy();
    m_card = nullptr;
}

void TourView::highlight(std::optional<app::TourTarget> target)
{
    m_frame.app().canvas().setHighlighted(target == app::TourTarget::CANVAS);
    const std::optional<SidePanel::Section> section =
        target ? sectionFor(*target) : std::optional<SidePanel::Section>();
    if (section)
    {
        m_frame.setSidePanelShown(true);
        m_frame.panel().scrollToSection(*section);
    }
    m_frame.panel().setHighlightedSection(section);
}

wxRect TourView::anchorFor(app::TourTarget target) const
{
    const std::optional<SidePanel::Section> section = sectionFor(target);
    if (!section)
    {
        return m_frame.canvas().GetRect();
    }
    const wxRect  inPanel = m_frame.panel().sectionRect(*section);
    const wxPoint origin =
        m_frame.ScreenToClient(m_frame.panel().ClientToScreen(inPanel.GetPosition()));
    return {origin, inPanel.GetSize()};
}

void TourView::learnMore()
{
    const app::TourScript& tour = m_frame.app().tour();
    if (tour.currentStep() < tour.stepCount())
    {
        m_frame.app().showHelpPage(tour.steps()[tour.currentStep()].helpPage);
    }
}

void TourView::onFrameResized(wxSizeEvent& event)
{
    event.Skip();
    if (m_card != nullptr && m_anchor)
    {
        // The sizer lays the canvas and panel out after this event; place the card once it has.
        m_frame.CallAfter([this] {
            if (m_card != nullptr && m_anchor)
            {
                m_card->placeNear(anchorFor(*m_anchor));
            }
        });
    }
}

}  // namespace mandelbrotter::gui
