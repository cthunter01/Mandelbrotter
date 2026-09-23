#pragma once

#include <functional>

#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/spinctrl.h>

#include "Mandelbrotter/exporter.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::gui
{

/// Asks for the output size and anti-aliasing level of a PNG export. Shown modeless by MainFrame,
/// which handles its OK and Cancel buttons.
class ExportDialog : public wxDialog
{
public:
    ExportDialog(wxWindow* parent, PixelSize initialSize);

    [[nodiscard]] ExportOptions options() const;

    /// F1 inside the dialog.
    std::function<void()> onHelp;

private:
    void onCharHook(wxKeyEvent& event);

    wxSpinCtrl* m_width{nullptr};
    wxSpinCtrl* m_height{nullptr};
    wxChoice*   m_supersample{nullptr};
};

}  // namespace mandelbrotter::gui
