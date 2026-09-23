#include "gui/DemoPlayer.h"

#include <chrono>
#include <cmath>
#include <format>
#include <utility>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/flights.h"

namespace mandelbrotter::gui
{

namespace
{

constexpr int kTickMs = 33;

}  // namespace

DemoPlayer::DemoPlayer(Hooks hooks) : m_hooks(std::move(hooks)), m_timer(this)
{
    Bind(wxEVT_TIMER, &DemoPlayer::onTick, this);
}

void DemoPlayer::play(const Flight& flight)
{
    if (flight.keyframes.empty())
    {
        return;
    }
    stop();
    m_flight      = &flight;
    m_start       = std::chrono::steady_clock::now();
    m_lastApplied = flight.keyframes.front().settings;
    m_hooks.applyFrame(m_lastApplied);
    m_hooks.showStatus(std::format("Flight: {} (Esc stops)", flight.title));
    m_timer.Start(kTickMs);
}

void DemoPlayer::stop()
{
    if (m_flight == nullptr)
    {
        return;
    }
    m_timer.Stop();
    m_flight = nullptr;
    m_hooks.showStatus("");
    if (m_hooks.finished)
    {
        m_hooks.finished();
    }
}

void DemoPlayer::onTick(wxTimerEvent& /*event*/)
{
    if (m_flight == nullptr)
    {
        return;
    }
    const Flight& flight  = *m_flight;
    const auto    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_start);
    const auto total = totalDuration(flight);
    if (elapsed >= total)
    {
        m_hooks.applyFrame(flight.keyframes.back().settings);
        stop();
        return;
    }
    if (m_hooks.canvasReady && !m_hooks.canvasReady())
    {
        return;  // the previous frame has not shown yet: drop this one, the clock keeps running
    }
    const RenderSettings frame = flightSettingsAt(flight, elapsed);
    if (frame != m_lastApplied)
    {
        m_lastApplied = frame;
        m_hooks.applyFrame(frame);
    }
    const int percent = static_cast<int>(std::lround(100.0 * static_cast<double>(elapsed.count()) /
                                                     static_cast<double>(total.count())));
    m_hooks.showStatus(std::format("Flight: {} {}% (Esc stops)", flight.title, percent));
}

}  // namespace mandelbrotter::gui
