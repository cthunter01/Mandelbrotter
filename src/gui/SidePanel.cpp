#include "gui/SidePanel.h"

#include <cmath>
#include <format>
#include <string>
#include <utility>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dc.h>
#include <wx/listbox.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/tglbtn.h>

#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/parse.h"
#include "gui/JuliaPreview.h"
#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

namespace
{

constexpr int kOffsetSliderSteps = 1000;
constexpr int kBorder            = 6;

constexpr std::size_t index(SidePanel::Section section)
{
    return static_cast<std::size_t>(section);
}

std::optional<double> parseDouble(const wxString& text)
{
    const std::string utf8 = fromWx(text);
    return parseNumber<double>(trimSpaces(utf8));
}

wxString formatDouble(double value)
{
    return toWx(std::format("{:.10g}", value));
}

wxStaticBoxSizer* section(wxWindow* parent, wxSizer& into, const char* title)
{
    auto* box = new wxStaticBoxSizer(wxVERTICAL, parent, title);
    into.Add(box, wxSizerFlags().Expand().Border(wxALL, kBorder));
    return box;
}

wxSizer* labelled(wxWindow* parent, const char* label, wxWindow* control)
{
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(parent, wxID_ANY, label),
             wxSizerFlags().CenterVertical().Border(wxRIGHT, kBorder));
    row->Add(control, wxSizerFlags(1).Expand());
    return row;
}

}  // namespace

SidePanel::SidePanel(wxWindow* parent) : wxScrolledWindow(parent, wxID_ANY)
{
    SetScrollRate(0, FromDIP(10));
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    buildFractalSection(*sizer);
    buildIterationSection(*sizer);
    buildColoringSection(*sizer);
    buildOverlaySection(*sizer);
    buildBookmarkSection(*sizer);
    SetSizer(sizer);
    // Wide enough for the widest row plus the vertical scrollbar; the height is whatever the frame
    // gives us.
    SetMinSize(wxSize(sizer->GetMinSize().x + FromDIP(24), FromDIP(200)));
    FitInside();
    setSettings(m_settings);
}

// ---------------------------------------------------------------------------------------------------------------
// Construction

void SidePanel::buildFractalSection(wxSizer& sizer)
{
    wxStaticBoxSizer* box                  = section(this, sizer, "Fractal");
    wxWindow*         owner                = box->GetStaticBox();
    m_sections.at(index(Section::FRACTAL)) = box->GetStaticBox();

    m_family = new wxChoice(owner, wxID_ANY);
    for (const FractalFamily family : allFamilies())
    {
        m_family->Append(toWx(displayName(family)));
    }
    m_family->Bind(wxEVT_CHOICE, [this](wxCommandEvent& event) {
        const auto families = allFamilies();
        const int  index    = event.GetSelection();
        if (index >= 0 && static_cast<std::size_t>(index) < families.size())
        {
            m_settings.fractal.family = families[static_cast<std::size_t>(index)];
            emitChange();
        }
    });
    box->Add(labelled(owner, "Family", m_family),
             wxSizerFlags().Expand().Border(wxALL, kBorder / 2));

    m_exponent = new wxSpinCtrl(owner, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                wxSP_ARROW_KEYS, kMinExponent, kMaxExponent, 2);
    m_exponent->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent& event) {
        m_settings.fractal.exponent = clampExponent(event.GetPosition());
        emitChange();
    });
    box->Add(labelled(owner, "Exponent n", m_exponent),
             wxSizerFlags().Expand().Border(wxALL, kBorder / 2));

    m_julia = new wxCheckBox(owner, wxID_ANY, "Julia set (z0 = pixel, c = seed)");
    m_julia->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& event) {
        m_settings.fractal.julia = event.IsChecked();
        emitChange();
    });
    box->Add(m_julia, wxSizerFlags().Border(wxALL, kBorder / 2));

    auto* seedRow = new wxBoxSizer(wxHORIZONTAL);
    m_seedRe =
        new wxTextCtrl(owner, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_seedIm =
        new wxTextCtrl(owner, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    for (wxTextCtrl* field : {m_seedRe, m_seedIm})
    {
        field->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { readSeedFields(); });
        field->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
            readSeedFields();
            event.Skip();
        });
    }
    seedRow->Add(new wxStaticText(owner, wxID_ANY, "c ="),
                 wxSizerFlags().CenterVertical().Border(wxRIGHT, kBorder));
    seedRow->Add(m_seedRe, wxSizerFlags(1).Expand());
    seedRow->Add(new wxStaticText(owner, wxID_ANY, "+"),
                 wxSizerFlags().CenterVertical().Border(wxLEFT | wxRIGHT, 4));
    seedRow->Add(m_seedIm, wxSizerFlags(1).Expand());
    seedRow->Add(new wxStaticText(owner, wxID_ANY, "i"),
                 wxSizerFlags().CenterVertical().Border(wxLEFT, 4));
    box->Add(seedRow, wxSizerFlags().Expand().Border(wxALL, kBorder / 2));

    m_pickSeed = new wxToggleButton(owner, wxID_ANY, "Pick seed from canvas");
    m_pickSeed->SetToolTip("Click a point of the set to use it as the Julia constant c");
    m_pickSeed->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent& event) {
        if (onPickSeedToggled)
        {
            onPickSeedToggled(event.IsChecked());
        }
    });
    box->Add(m_pickSeed, wxSizerFlags().Expand().Border(wxALL, kBorder / 2));

    box->Add(new wxStaticText(owner, wxID_ANY, "Julia preview (follows the mouse)"),
             wxSizerFlags().Border(wxLEFT | wxTOP, kBorder / 2));
    m_preview = new JuliaPreview(owner);
    box->Add(m_preview, wxSizerFlags().Expand().Border(wxALL, kBorder / 2));
}

void SidePanel::buildIterationSection(wxSizer& sizer)
{
    wxStaticBoxSizer* box                     = section(this, sizer, "Iterations");
    wxWindow*         owner                   = box->GetStaticBox();
    m_sections.at(index(Section::ITERATIONS)) = box->GetStaticBox();

    m_iterations =
        new wxSpinCtrl(owner, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
                       kMinIterations, kMaxIterations, kDefaultIterations);
    m_iterations->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent& event) {
        m_settings.maxIterations = event.GetPosition();
        emitChange();
    });
    box->Add(labelled(owner, "Maximum", m_iterations),
             wxSizerFlags().Expand().Border(wxALL, kBorder / 2));

    m_autoIterations = new wxCheckBox(owner, wxID_ANY, "Auto: grow with zoom");
    m_autoIterations->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& event) {
        m_settings.autoIterations = event.IsChecked();
        emitChange();
    });
    box->Add(m_autoIterations, wxSizerFlags().Border(wxALL, kBorder / 2));

    m_effectiveIterations = new wxStaticText(owner, wxID_ANY, "");
    box->Add(m_effectiveIterations, wxSizerFlags().Border(wxALL, kBorder / 2));
}

void SidePanel::buildColoringSection(wxSizer& sizer)
{
    wxStaticBoxSizer* box                    = section(this, sizer, "Colouring");
    wxWindow*         owner                  = box->GetStaticBox();
    m_sections.at(index(Section::COLOURING)) = box->GetStaticBox();

    m_palette = new wxChoice(owner, wxID_ANY);
    for (const auto name : paletteNames())
    {
        m_palette->Append(toWx(name));
    }
    m_palette->Bind(wxEVT_CHOICE, [this](wxCommandEvent& event) {
        const auto names = paletteNames();
        const int  index = event.GetSelection();
        if (index >= 0 && static_cast<std::size_t>(index) < names.size())
        {
            m_settings.coloring.palette = std::string(names[static_cast<std::size_t>(index)]);
            emitChange();
        }
    });
    box->Add(labelled(owner, "Palette", m_palette),
             wxSizerFlags().Expand().Border(wxALL, kBorder / 2));

    m_density = new wxSpinCtrlDouble(owner, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS, kMinDensity, kMaxDensity, 64.0, 1.0);
    m_density->SetDigits(1);
    m_density->SetToolTip("Iterations per palette cycle");
    m_density->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent& event) {
        m_settings.coloring.density = event.GetValue();
        emitChange();
    });
    box->Add(labelled(owner, "Density", m_density),
             wxSizerFlags().Expand().Border(wxALL, kBorder / 2));

    m_offset = new wxSlider(owner, wxID_ANY, 0, 0, kOffsetSliderSteps);
    m_offset->SetToolTip("Palette phase shift");
    m_offset->Bind(wxEVT_SLIDER, [this](wxCommandEvent& event) {
        m_settings.coloring.offset = static_cast<double>(event.GetInt()) / kOffsetSliderSteps;
        emitChange();
    });
    box->Add(labelled(owner, "Offset", m_offset),
             wxSizerFlags().Expand().Border(wxALL, kBorder / 2));
}

void SidePanel::buildOverlaySection(wxSizer& sizer)
{
    wxStaticBoxSizer* box                  = section(this, sizer, "Overlay");
    m_sections.at(index(Section::OVERLAY)) = box->GetStaticBox();
    m_orbit =
        new wxCheckBox(box->GetStaticBox(), wxID_ANY, "Show orbit of the point under the cursor");
    m_orbit->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& event) {
        if (onOrbitToggled)
        {
            onOrbitToggled(event.IsChecked());
        }
    });
    box->Add(m_orbit, wxSizerFlags().Border(wxALL, kBorder / 2));
}

void SidePanel::buildBookmarkSection(wxSizer& sizer)
{
    wxStaticBoxSizer* box                    = section(this, sizer, "Bookmarks");
    wxWindow*         owner                  = box->GetStaticBox();
    m_sections.at(index(Section::BOOKMARKS)) = box->GetStaticBox();

    m_bookmarks = new wxListBox(owner, wxID_ANY);
    m_bookmarks->SetMinSize(FromDIP(wxSize(-1, 120)));
    m_bookmarks->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) { syncEnabledState(); });
    m_bookmarks->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent& event) {
        if (onBookmarkLoad && event.GetSelection() != wxNOT_FOUND)
        {
            onBookmarkLoad(static_cast<std::size_t>(event.GetSelection()));
        }
    });
    box->Add(m_bookmarks, wxSizerFlags(1).Expand().Border(wxALL, kBorder / 2));

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    auto* add     = new wxButton(owner, wxID_ANY, "Add...");
    add->SetToolTip("Bookmark the current view");
    add->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (onBookmarkAdd)
        {
            onBookmarkAdd();
        }
    });
    m_loadBookmark = new wxButton(owner, wxID_ANY, "Load");
    m_loadBookmark->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        const int index = m_bookmarks->GetSelection();
        if (onBookmarkLoad && index != wxNOT_FOUND)
        {
            onBookmarkLoad(static_cast<std::size_t>(index));
        }
    });
    m_deleteBookmark = new wxButton(owner, wxID_ANY, "Delete");
    m_deleteBookmark->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        const int index = m_bookmarks->GetSelection();
        if (onBookmarkDelete && index != wxNOT_FOUND)
        {
            onBookmarkDelete(static_cast<std::size_t>(index));
        }
    });
    buttons->Add(add, wxSizerFlags(1).Border(wxRIGHT, kBorder / 2));
    buttons->Add(m_loadBookmark, wxSizerFlags(1).Border(wxRIGHT, kBorder / 2));
    buttons->Add(m_deleteBookmark, wxSizerFlags(1));
    box->Add(buttons, wxSizerFlags().Expand().Border(wxALL, kBorder / 2));
}

// ---------------------------------------------------------------------------------------------------------------
// Model <-> controls

void SidePanel::setSettings(const RenderSettings& settings)
{
    m_updating = true;
    m_settings = settings;

    const auto families = allFamilies();
    for (std::size_t i = 0; i < families.size(); ++i)
    {
        if (families[i] == settings.fractal.family)
        {
            m_family->SetSelection(static_cast<int>(i));
        }
    }
    m_exponent->SetValue(settings.fractal.exponent);
    m_julia->SetValue(settings.fractal.julia);
    m_seedRe->ChangeValue(formatDouble(settings.fractal.seed.re));
    m_seedIm->ChangeValue(formatDouble(settings.fractal.seed.im));
    m_iterations->SetValue(settings.maxIterations);
    m_autoIterations->SetValue(settings.autoIterations);
    setEffectiveIterations(effectiveIterations(settings));

    const auto names = paletteNames();
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        if (names[i] == settings.coloring.palette)
        {
            m_palette->SetSelection(static_cast<int>(i));
        }
    }
    m_density->SetValue(settings.coloring.density);
    const double phase = settings.coloring.offset - std::floor(settings.coloring.offset);
    m_offset->SetValue(static_cast<int>(std::lround(phase * kOffsetSliderSteps)));

    m_preview->setFractal(settings.fractal);
    m_preview->setColoring(settings.coloring);
    if (settings.fractal.julia)
    {
        m_preview->setSeed(settings.fractal.seed);
    }
    syncEnabledState();
    m_updating = false;
}

void SidePanel::setEffectiveIterations(int iterations)
{
    m_effectiveIterations->SetLabel(toWx(std::format("Effective limit: {}", iterations)));
}

void SidePanel::setBookmarks(std::span<const Bookmark> bookmarks)
{
    const int selected = m_bookmarks->GetSelection();
    m_bookmarks->Clear();
    for (const Bookmark& bookmark : bookmarks)
    {
        m_bookmarks->Append(toWx(bookmark.name));
    }
    if (selected != wxNOT_FOUND && std::cmp_less(selected, bookmarks.size()))
    {
        m_bookmarks->SetSelection(selected);
    }
    syncEnabledState();
}

void SidePanel::setPickSeedMode(bool enabled)
{
    m_pickSeed->SetValue(enabled);
}

void SidePanel::setShowOrbit(bool enabled)
{
    m_orbit->SetValue(enabled);
}

void SidePanel::setPreviewSeed(std::optional<Complex> seed)
{
    if (m_settings.fractal.julia)
    {
        m_preview->setSeed(m_settings.fractal.seed);
    }
    else
    {
        m_preview->setSeed(seed);
    }
}

void SidePanel::readSeedFields()
{
    if (m_updating)
    {
        return;
    }
    const std::optional<double> re = parseDouble(m_seedRe->GetValue());
    const std::optional<double> im = parseDouble(m_seedIm->GetValue());
    if (!re || !im)
    {
        // Restore the last valid values rather than guessing.
        m_seedRe->ChangeValue(formatDouble(m_settings.fractal.seed.re));
        m_seedIm->ChangeValue(formatDouble(m_settings.fractal.seed.im));
        return;
    }
    const Complex seed{*re, *im};
    if (seed == m_settings.fractal.seed)
    {
        return;
    }
    m_settings.fractal.seed = seed;
    emitChange();
}

void SidePanel::emitChange()
{
    if (m_updating)
    {
        return;
    }
    syncEnabledState();
    if (onSettingsChanged)
    {
        onSettingsChanged(m_settings);
    }
}

void SidePanel::syncEnabledState()
{
    const bool hasSelection = m_bookmarks->GetSelection() != wxNOT_FOUND;
    m_loadBookmark->Enable(hasSelection);
    m_deleteBookmark->Enable(hasSelection);
}

// ---------------------------------------------------------------------------------------------------------------
// Sections (context help, the tour)

wxStaticBox* SidePanel::box(Section section) const
{
    return m_sections.at(index(section));
}

std::optional<SidePanel::Section> SidePanel::sectionOf(wxWindow* window) const
{
    if (window == nullptr)
    {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < kSectionCount; ++i)
    {
        const wxStaticBox* candidate = m_sections.at(i);
        if (candidate != nullptr && (candidate == window || candidate->IsDescendant(window)))
        {
            return static_cast<Section>(i);
        }
    }
    return std::nullopt;
}

bool SidePanel::isJuliaControl(const wxWindow* window) const
{
    return window == m_julia || window == m_seedRe || window == m_seedIm || window == m_pickSeed ||
           window == m_preview;
}

wxRect SidePanel::sectionRect(Section section) const
{
    return box(section)->GetRect();
}

void SidePanel::scrollToSection(Section section)
{
    const wxRect rect   = sectionRect(section);
    const wxSize client = GetClientSize();
    if (rect.GetTop() >= 0 && rect.GetBottom() <= client.y)
    {
        return;  // already in view
    }
    int unitY = 1;
    GetScrollPixelsPerUnit(nullptr, &unitY);
    const wxPoint unscrolled = CalcUnscrolledPosition(rect.GetPosition());
    Scroll(wxDefaultCoord, std::max(0, (unscrolled.y - kBorder) / std::max(1, unitY)));
    Refresh();  // GTK may leave the static boxes' frames and titles undrawn after a long scroll
}

void SidePanel::setHighlightedSection(std::optional<Section> section)
{
    if (m_highlighted == section)
    {
        return;
    }
    m_highlighted = section;
    Refresh();
}

void SidePanel::OnDraw(wxDC& dc)
{
    if (!m_highlighted)
    {
        return;
    }
    // The DC is already offset by the scroll position, so draw in unscrolled coordinates. The
    // ring sits in the margin around the box (kBorder); the controls draw over the rest.
    const wxRect rect = box(*m_highlighted)->GetRect();
    wxRect       ring(CalcUnscrolledPosition(rect.GetPosition()), rect.GetSize());
    ring.Inflate(FromDIP(3));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT), FromDIP(3)));
    dc.DrawRoundedRectangle(ring, FromDIP(4));
}

}  // namespace mandelbrotter::gui
