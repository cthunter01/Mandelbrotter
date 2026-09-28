#include "gui/TourCard.h"

#include <cstddef>
#include <string_view>

#include <wx/dcbuffer.h>
#include <wx/settings.h>
#include <wx/sizer.h>

#include "Mandelbrotter/app/tour_layout.h"
#include "Mandelbrotter/app/ui_text.h"
#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

TourCard::TourCard(wxWindow* parent)
  : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));
    SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOTEXT));
    Bind(wxEVT_PAINT, &TourCard::onPaint, this);

    m_title = new wxStaticText(this, wxID_ANY, "");
    m_title->SetFont(GetFont().Bold().Larger());
    m_text     = new wxStaticText(this, wxID_ANY, "");
    m_progress = new wxStaticText(this, wxID_ANY, "");

    m_learnMore = new wxButton(this, wxID_ANY, toWx(app::kLearnMore));
    auto* close = new wxButton(this, wxID_ANY, toWx(app::kClose));
    m_back      = new wxButton(this, wxID_ANY, toWx(app::kBack));
    m_next      = new wxButton(this, wxID_ANY, toWx(app::kNext));
    m_learnMore->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (onLearnMore)
        {
            onLearnMore();
        }
    });
    close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (onClose)
        {
            onClose();
        }
    });
    m_back->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (onBack)
        {
            onBack();
        }
    });
    m_next->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (onNext)
        {
            onNext();
        }
    });

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    buttons->Add(m_learnMore, wxSizerFlags().Border(wxRIGHT, FromDIP(app::kCardButtonGap)));
    buttons->Add(close);
    buttons->AddStretchSpacer();
    buttons->Add(m_back, wxSizerFlags().Border(wxRIGHT, FromDIP(app::kCardButtonGap)));
    buttons->Add(m_next);

    auto*     sizer   = new wxBoxSizer(wxVERTICAL);
    const int padding = FromDIP(app::kCardPadding);
    sizer->Add(m_title, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, padding));
    sizer->Add(m_text, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, padding));
    sizer->Add(m_progress, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, padding));
    sizer->Add(buttons, wxSizerFlags().Expand().Border(wxALL, padding));
    SetSizerAndFit(sizer);
}

void TourCard::setStep(std::string_view title, std::string_view text, std::size_t index,
                       std::size_t count, bool hasHelpPage)
{
    m_title->SetLabel(toWx(title));
    m_text->SetLabel(toWx(text));
    m_text->Wrap(FromDIP(app::kCardTextWidth));
    m_progress->SetLabel(toWx(app::stepText(index, count)));
    m_back->Enable(index > 0);
    m_next->SetLabel(toWx(index + 1 == count ? app::kFinish : app::kNext));
    m_learnMore->Show(hasHelpPage);
    Layout();
    Fit();
}

void TourCard::placeNear(const wxRect& anchor)
{
    SetPosition(
        toWx(app::placeTourCard(fromWx(anchor), fromWx(GetSize()),
                                fromWx(GetParent()->GetClientSize()), FromDIP(app::kCardGap))));
    Raise();
    Show();
}

void TourCard::onPaint(wxPaintEvent& /*event*/)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetPen(
        wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT), FromDIP(app::kCardBorder)));
    dc.DrawRectangle(GetClientRect());
}

}  // namespace mandelbrotter::gui
