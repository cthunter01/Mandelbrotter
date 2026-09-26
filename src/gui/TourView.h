#pragma once

#include <cstddef>
#include <optional>

#include <wx/event.h>
#include <wx/gdicmn.h>

#include "Mandelbrotter/app/TourScript.h"

namespace mandelbrotter::gui
{

class MainFrame;
class TourCard;

/// The guided tour on screen: app::TourScript runs the steps, this shows them. It places the step's
/// card beside its anchor (again whenever the frame is resized), highlights the canvas or a side
/// panel section, and routes the card's buttons back to the script.
class TourView : public wxEvtHandler
{
public:
    explicit TourView(MainFrame& frame);
    ~TourView() override;
    TourView(const TourView&)            = delete;
    TourView& operator=(const TourView&) = delete;
    TourView(TourView&&)                 = delete;
    TourView& operator=(TourView&&)      = delete;

    // app::TourScript::Hooks
    void showCard(const app::TourStep& step, std::size_t index, std::size_t count);
    void hideCard();
    void highlight(std::optional<app::TourTarget> target);

private:
    [[nodiscard]] wxRect anchorFor(app::TourTarget target) const;
    void                 learnMore();
    void                 onFrameResized(wxSizeEvent& event);

    MainFrame&                     m_frame;
    TourCard*                      m_card{nullptr};
    std::optional<app::TourTarget> m_anchor;  ///< where the card sits, while it is shown
};

}  // namespace mandelbrotter::gui
