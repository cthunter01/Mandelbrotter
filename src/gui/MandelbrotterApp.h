#pragma once

#include <filesystem>
#include <optional>

#include <wx/app.h>

#include "Mandelbrotter/RenderSettings.h"

namespace mandelbrotter::gui
{

/// What main() hands to the GUI before wxEntry: the view to open with and, for the developer
/// screenshot mode (--screenshots DIR), where to write the help book's pictures.
struct StartupOptions
{
    RenderSettings                       settings;
    std::optional<std::filesystem::path> screenshotsDir;
};

void                                setStartupOptions(StartupOptions options);
[[nodiscard]] const StartupOptions& startupOptions();

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
