#pragma once

#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/spinctrl.h>

#include "Mandelbrotter/exporter.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::gui
{

/// Asks for the output size and anti-aliasing level of a PNG export.
class ExportDialog : public wxDialog
{
public:
    ExportDialog(wxWindow* parent, PixelSize initialSize);

    [[nodiscard]] ExportOptions options() const;

private:
    wxSpinCtrl* m_width{nullptr};
    wxSpinCtrl* m_height{nullptr};
    wxChoice*   m_supersample{nullptr};
};

}  // namespace mandelbrotter::gui
