#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/timer.h>
#include <wx/window.h>

#include "Mandelbrotter/app/ScreenshotScript.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::gui
{

class MainFrame;

/// The developer screenshot mode (--screenshots DIR) on wx: app::ScreenshotScript walks the window
/// through the states the help book shows; this fills its hooks with wx's timers, dialogs and
/// window capture (window_capture.h), and closes the window when the script is done.
class ScreenshotRun : public wxEvtHandler
{
public:
    ScreenshotRun(MainFrame& frame, std::filesystem::path dir);
    ~ScreenshotRun() override;
    ScreenshotRun(const ScreenshotRun&)            = delete;
    ScreenshotRun& operator=(const ScreenshotRun&) = delete;
    ScreenshotRun(ScreenshotRun&&)                 = delete;
    ScreenshotRun& operator=(ScreenshotRun&&)      = delete;

    void start();

private:
    [[nodiscard]] app::ScreenshotScript::Hooks makeHooks();
    [[nodiscard]] wxWindow*                    targetWindow(app::ShotTarget target);
    /// The region's rectangle in screen coordinates, with the margin; nullopt for the whole target.
    [[nodiscard]] std::optional<wxRect>                 regionRect(app::ShotRegion region);
    [[nodiscard]] std::expected<PixelSize, std::string> capture(app::ShotTarget              target,
                                                                app::ShotRegion              region,
                                                                const std::filesystem::path& path);
    void                                                closeBookmarkDialog();

    MainFrame&            m_frame;
    wxTimer               m_settle;
    wxTimer               m_watchdog;
    wxDialog*             m_dialog{nullptr};  ///< the Add bookmark dialog of its shot
    app::ScreenshotScript m_script;           ///< last: its hooks use the members above
};

}  // namespace mandelbrotter::gui
