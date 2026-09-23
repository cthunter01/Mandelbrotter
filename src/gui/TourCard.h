#pragma once

#include <cstddef>
#include <functional>
#include <string_view>

#include <wx/button.h>
#include <wx/gdicmn.h>
#include <wx/panel.h>
#include <wx/stattext.h>

namespace mandelbrotter::gui
{

/// The guided tour's speech bubble: title, text, "Step i of n" and Back / Next / Close / Learn
/// more. A child of the main frame outside its sizer, placed beside whatever the step points at
/// (a child window needs no top-level positioning, which Wayland does not allow).
class TourCard : public wxPanel
{
public:
    explicit TourCard(wxWindow* parent);

    void setStep(std::string_view title, std::string_view text, std::size_t index,
                 std::size_t count, bool hasHelpPage);
    /// Places the card beside `anchor` (parent client coordinates): to its left when there is
    /// room, else to its right, or inside it when the anchor is a large area such as the canvas.
    void placeNear(const wxRect& anchor);

    std::function<void()> onBack;
    std::function<void()> onNext;
    std::function<void()> onClose;
    std::function<void()> onLearnMore;

private:
    void onPaint(wxPaintEvent& event);

    wxStaticText* m_title{nullptr};
    wxStaticText* m_text{nullptr};
    wxStaticText* m_progress{nullptr};
    wxButton*     m_learnMore{nullptr};
    wxButton*     m_back{nullptr};
    wxButton*     m_next{nullptr};
};

}  // namespace mandelbrotter::gui
