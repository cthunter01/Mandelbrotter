#pragma once

#include <chrono>
#include <functional>
#include <string_view>

#include <wx/event.h>
#include <wx/timer.h>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/flights.h"

namespace mandelbrotter::gui
{

/// Plays a Flight on the main window: a timer samples the flight at wall-clock time and hands each
/// frame to the canvas, so a flight always takes its scripted duration; a slow machine just sees
/// coarser frames. A frame is only pushed once the previous one has shown its coarse pass, because
/// starting a render waits for the running one to stop.
class DemoPlayer : public wxEvtHandler
{
public:
    struct Hooks
    {
        std::function<void(const RenderSettings&)> applyFrame;  ///< show this frame now
        std::function<bool()>                 canvasReady;  ///< the last frame's coarse pass is up
        std::function<void(std::string_view)> showStatus;   ///< progress text ("" when done)
        std::function<void()>                 finished;     ///< the flight ended or was stopped
    };

    explicit DemoPlayer(Hooks hooks);

    /// Jumps to the flight's first keyframe and starts playing (stopping any flight in progress).
    void play(const Flight& flight);
    /// Stops where the flight is; the view stays. No effect when nothing plays.
    void stop();

    [[nodiscard]] bool          playing() const noexcept { return m_flight != nullptr; }
    [[nodiscard]] const Flight* current() const noexcept { return m_flight; }

private:
    void onTick(wxTimerEvent& event);

    Hooks                                 m_hooks;
    wxTimer                               m_timer;
    const Flight*                         m_flight{nullptr};
    std::chrono::steady_clock::time_point m_start;
    RenderSettings                        m_lastApplied;
};

}  // namespace mandelbrotter::gui
