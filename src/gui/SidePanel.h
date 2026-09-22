#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <span>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/listbox.h>
#include <wx/scrolwin.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/tglbtn.h>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::gui
{

class JuliaPreview;

/// The controls beside the canvas. Edits are reported as complete RenderSettings through
/// onSettingsChanged; setSettings() pushes the model back into the controls without echoing.
class SidePanel : public wxScrolledWindow
{
public:
    explicit SidePanel(wxWindow* parent);

    void setSettings(const RenderSettings& settings);
    void setEffectiveIterations(int iterations);
    void setBookmarks(std::span<const Bookmark> bookmarks);
    void setPickSeedMode(bool enabled);
    void setShowOrbit(bool enabled);
    /// The seed the Julia preview follows (the hovered point, or the current seed in Julia mode).
    void setPreviewSeed(std::optional<Complex> seed);

    std::function<void(const RenderSettings&)> onSettingsChanged;
    std::function<void(bool)>                  onPickSeedToggled;
    std::function<void(bool)>                  onOrbitToggled;
    std::function<void()>                      onBookmarkAdd;
    std::function<void(std::size_t)>           onBookmarkLoad;
    std::function<void(std::size_t)>           onBookmarkDelete;

private:
    void buildFractalSection(wxSizer& sizer);
    void buildIterationSection(wxSizer& sizer);
    void buildColoringSection(wxSizer& sizer);
    void buildOverlaySection(wxSizer& sizer);
    void buildBookmarkSection(wxSizer& sizer);

    void readSeedFields();
    void emitChange();
    void syncEnabledState();

    RenderSettings m_settings;
    bool           m_updating{false};

    wxChoice*         m_family{nullptr};
    wxSpinCtrl*       m_exponent{nullptr};
    wxCheckBox*       m_julia{nullptr};
    wxTextCtrl*       m_seedRe{nullptr};
    wxTextCtrl*       m_seedIm{nullptr};
    wxToggleButton*   m_pickSeed{nullptr};
    JuliaPreview*     m_preview{nullptr};
    wxSpinCtrl*       m_iterations{nullptr};
    wxCheckBox*       m_autoIterations{nullptr};
    wxStaticText*     m_effectiveIterations{nullptr};
    wxChoice*         m_palette{nullptr};
    wxSpinCtrlDouble* m_density{nullptr};
    wxSlider*         m_offset{nullptr};
    wxCheckBox*       m_orbit{nullptr};
    wxListBox*        m_bookmarks{nullptr};
    wxButton*         m_loadBookmark{nullptr};
    wxButton*         m_deleteBookmark{nullptr};
};

}  // namespace mandelbrotter::gui
