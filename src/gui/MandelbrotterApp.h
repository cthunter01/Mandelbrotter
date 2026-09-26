#pragma once

#include <wx/app.h>

#include "Mandelbrotter/app/startup.h"

namespace mandelbrotter::gui
{

/// What main() hands to the GUI before wxEntry (app::StartupOptions).
void                                     setStartupOptions(app::StartupOptions options);
[[nodiscard]] const app::StartupOptions& startupOptions();

class MandelbrotterApp : public wxApp
{
public:
    bool OnInit() override;
    /// The main loop's result, or the code set with setExitCode() when the loop ended normally.
    int OnRun() override;

    /// The process exit code once the window has closed (the screenshot mode reports failures).
    void setExitCode(int code) noexcept { m_exitCode = code; }

private:
    int m_exitCode{0};
};

}  // namespace mandelbrotter::gui
