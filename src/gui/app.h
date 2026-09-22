#pragma once

#include <wx/app.h>

#include "Mandelbrotter/render_settings.h"

namespace mandelbrotter::gui
{

/// The view the window opens with. Set by main() before wxEntry from the command line.
void                                setStartupSettings(const RenderSettings& settings);
[[nodiscard]] const RenderSettings& startupSettings();

class MandelbrotterApp : public wxApp
{
public:
    bool OnInit() override;
};

}  // namespace mandelbrotter::gui
