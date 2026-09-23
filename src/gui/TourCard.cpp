#include "gui/TourCard.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <string_view>

#include <wx/dcbuffer.h>
#include <wx/settings.h>
#include <wx/sizer.h>

#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

namespace
{

constexpr int kTextWidthDip = 300;
constexpr int kPaddingDip   = 12;
constexpr int kGapDip       = 12;

}  // namespace

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

    m_learnMore = new wxButton(this, wxID_ANY, "Learn more");
    auto* close = new wxButton(this, wxID_ANY, "Close");
    m_back      = new wxButton(this, wxID_ANY, "Back");
    m_next      = new wxButton(this, wxID_ANY, "Next");
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
    buttons->Add(m_learnMore, wxSizerFlags().Border(wxRIGHT, FromDIP(4)));
    buttons->Add(close);
    buttons->AddStretchSpacer();
    buttons->Add(m_back, wxSizerFlags().Border(wxRIGHT, FromDIP(4)));
    buttons->Add(m_next);

    auto*     sizer   = new wxBoxSizer(wxVERTICAL);
    const int padding = FromDIP(kPaddingDip);
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
    m_text->Wrap(FromDIP(kTextWidthDip));
    m_progress->SetLabel(toWx(std::format("Step {} of {}", index + 1, count)));
    m_back->Enable(index > 0);
    m_next->SetLabel(index + 1 == count ? "Finish" : "Next");
    m_learnMore->Show(hasHelpPage);
    Layout();
    Fit();
}

void TourCard::placeNear(const wxRect& anchor)
{
    const wxSize client = GetParent()->GetClientSize();
    const wxSize size   = GetSize();
    const int    gap    = FromDIP(kGapDip);

    wxPoint at;
    if (anchor.width > 2 * size.x)
    {
        // A large area (the canvas): sit inside its top-left corner.
        at = wxPoint(anchor.x + gap, anchor.y + gap);
    }
    else if (anchor.x - size.x - gap >= 0)
    {
        at = wxPoint(anchor.x - size.x - gap, anchor.y);
    }
    else
    {
        at = wxPoint(anchor.GetRight() + gap, anchor.y);
    }
    at.x = std::clamp(at.x, 0, std::max(0, client.x - size.x));
    at.y = std::clamp(at.y, 0, std::max(0, client.y - size.y));
    SetPosition(at);
    Raise();
    Show();
}

void TourCard::onPaint(wxPaintEvent& /*event*/)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT), FromDIP(2)));
    dc.DrawRectangle(GetClientRect());
}

}  // namespace mandelbrotter::gui
