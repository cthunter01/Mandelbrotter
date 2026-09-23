#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/timer.h>
#include <wx/window.h>

namespace mandelbrotter::gui
{

class MainFrame;

/// The developer screenshot mode (--screenshots DIR): walks the window through the states the help
/// book shows, saves each as a PNG under DIR, then closes the window. A shot is taken after the
/// canvas has finished rendering and a short pause for the toolkit to repaint. Failures are
/// printed and turn into exit code 1.
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
    struct Shot
    {
        std::string                file;
        bool                       rendersFirst;  ///< wait for the canvas after prepare()
        std::function<void()>      prepare;
        std::function<wxWindow*()> target;            ///< the window to capture
        std::function<std::optional<wxRect>()> crop;  ///< screen rectangle to keep, if any
        std::function<void()>                  cleanup;
    };

    [[nodiscard]] std::vector<Shot> buildShots();
    void                            runCurrent();
    void                            onRenderFinished();
    void                            settle();
    void                            onSettled(wxTimerEvent& event);
    void                            onWatchdog(wxTimerEvent& event);
    void                            capture();
    void                            fail(const std::string& why);
    void                            finish(int exitCode);

    MainFrame&            m_frame;
    std::filesystem::path m_dir;
    std::vector<Shot>     m_shots;
    std::size_t           m_index{0};
    wxTimer               m_settle;
    wxTimer               m_watchdog;
    bool                  m_waitingForRender{false};
    bool                  m_done{false};
    wxDialog*             m_dialog{nullptr};  ///< a dialog created for one shot
};

}  // namespace mandelbrotter::gui
