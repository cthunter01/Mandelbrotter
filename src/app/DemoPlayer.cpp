#include "Mandelbrotter/app/DemoPlayer.h"

#include <chrono>
#include <cmath>
#include <format>
#include <utility>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/flights.h"

namespace mandelbrotter::app
{

DemoPlayer::DemoPlayer(Hooks hooks) : m_hooks(std::move(hooks)) { }

std::chrono::steady_clock::time_point DemoPlayer::now() const
{
    return m_hooks.now ? m_hooks.now() : std::chrono::steady_clock::now();
}

void DemoPlayer::play(const Flight& flight)
{
    if (flight.keyframes.empty())
    {
        return;
    }
    stop();
    m_flight      = &flight;
    m_start       = now();
    m_lastApplied = flight.keyframes.front().settings;
    m_hooks.applyFrame(m_lastApplied);
    m_hooks.showStatus(std::format("Flight: {} (Esc stops)", flight.title));
    if (m_hooks.setTimerRunning)
    {
        m_hooks.setTimerRunning(true);
    }
}

void DemoPlayer::stop()
{
    if (m_flight == nullptr)
    {
        return;
    }
    if (m_hooks.setTimerRunning)
    {
        m_hooks.setTimerRunning(false);
    }
    m_flight = nullptr;
    m_hooks.showStatus("");
    if (m_hooks.finished)
    {
        m_hooks.finished();
    }
}

void DemoPlayer::tick()
{
    if (m_flight == nullptr)
    {
        return;
    }
    const Flight& flight  = *m_flight;
    const auto    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now() - m_start);
    const auto    total   = totalDuration(flight);
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

}  // namespace mandelbrotter::app
