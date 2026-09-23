#include "gui/ExportDialog.h"

#include <algorithm>
#include <array>

#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

namespace mandelbrotter::gui
{

namespace
{

constexpr std::array kSupersampleChoices{1, 2, 4};

}  // namespace

ExportDialog::ExportDialog(wxWindow* parent, PixelSize initialSize)
  : wxDialog(parent, wxID_ANY, "Save image as PNG", wxDefaultPosition, wxDefaultSize,
             wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    auto* grid = new wxFlexGridSizer(2, FromDIP(wxSize(8, 6)));
    grid->AddGrowableCol(1);

    m_width =
        new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1,
                       kMaxExportDimension, std::clamp(initialSize.width, 1, kMaxExportDimension));
    m_height =
        new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1,
                       kMaxExportDimension, std::clamp(initialSize.height, 1, kMaxExportDimension));
    m_supersample = new wxChoice(this, wxID_ANY);
    m_supersample->Append("1x (none)");
    m_supersample->Append("2x2 samples per pixel");
    m_supersample->Append("4x4 samples per pixel");
    m_supersample->SetSelection(0);

    grid->Add(new wxStaticText(this, wxID_ANY, "Width (px)"), wxSizerFlags().CenterVertical());
    grid->Add(m_width, wxSizerFlags().Expand());
    grid->Add(new wxStaticText(this, wxID_ANY, "Height (px)"), wxSizerFlags().CenterVertical());
    grid->Add(m_height, wxSizerFlags().Expand());
    grid->Add(new wxStaticText(this, wxID_ANY, "Anti-aliasing"), wxSizerFlags().CenterVertical());
    grid->Add(m_supersample, wxSizerFlags().Expand());

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(grid, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(12)));
    sizer->Add(new wxStaticText(
                   this, wxID_ANY,
                   "The view keeps its centre and zoom; the shorter side shows the same extent."),
               wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12)));
    sizer->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL),
               wxSizerFlags().Expand().Border(wxALL, FromDIP(8)));
    SetSizerAndFit(sizer);
    Bind(wxEVT_CHAR_HOOK, &ExportDialog::onCharHook, this);
}

// Not const: wxEvtHandler::Bind takes a non-const member function.
void ExportDialog::onCharHook(wxKeyEvent& event)  // NOLINT(readability-make-member-function-const)
{
    if (event.GetKeyCode() == WXK_F1 && onHelp)
    {
        onHelp();
        return;
    }
    event.Skip();
}

ExportOptions ExportDialog::options() const
{
    const int choice = std::clamp(m_supersample->GetSelection(), 0,
                                  static_cast<int>(kSupersampleChoices.size()) - 1);
    return {.size        = {m_width->GetValue(), m_height->GetValue()},
            .supersample = kSupersampleChoices.at(static_cast<std::size_t>(choice))};
}

}  // namespace mandelbrotter::gui
