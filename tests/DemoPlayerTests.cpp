#include "Mandelbrotter/app/DemoPlayer.h"

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/flights.h"
#include "Mandelbrotter/geometry.h"

namespace
{

using mandelbrotter::Flight;
using mandelbrotter::RenderSettings;
using mandelbrotter::app::DemoPlayer;
using std::chrono::milliseconds;

RenderSettings at(mandelbrotter::Complex center, double zoom)
{
    RenderSettings settings;
    settings.view.zoom = zoom;
    settings.view.center =
        mandelbrotter::BigComplex::fromComplex(center, mandelbrotter::fractionBitsFor(zoom));
    return settings;
}

/// Two seconds: a one-second zoom from 1x to 16x, then a one-second pan.
Flight testFlight()
{
    return {.id          = "test",
            .title       = "Test flight",
            .description = "",
            .keyframes   = {{.settings = at({-0.5, 0.0}, 1.0)},
                            {.settings = at({-0.5, 0.0}, 16.0),
                             .duration = milliseconds(1000),
                             .easing   = mandelbrotter::Easing::LINEAR},
                            {.settings = at({-0.25, 0.0}, 16.0),
                             .duration = milliseconds(1000),
                             .easing   = mandelbrotter::Easing::LINEAR}}};
}

/// Records every hook call; the clock only moves when a test advances it.
struct Recorder
{
    std::vector<RenderSettings>           frames;
    std::vector<std::string>              statuses;
    std::vector<bool>                     timer;
    int                                   finished{0};
    bool                                  ready{true};
    std::chrono::steady_clock::time_point clock;

    void advance(milliseconds by) { clock += by; }

    [[nodiscard]] DemoPlayer::Hooks hooks()
    {
        return {.applyFrame      = [this](const RenderSettings& s) { frames.push_back(s); },
                .canvasReady     = [this] { return ready; },
                .showStatus      = [this](std::string_view s) { statuses.emplace_back(s); },
                .finished        = [this] { ++finished; },
                .setTimerRunning = [this](bool on) { timer.push_back(on); },
                .now             = [this] { return clock; }};
    }
};

TEST(DemoPlayer, PlayShowsTheFirstKeyframeAndStartsTheTimer)
{
    Recorder     rec;
    DemoPlayer   player(rec.hooks());
    const Flight flight = testFlight();
    player.play(flight);
    EXPECT_TRUE(player.playing());
    EXPECT_EQ(player.current(), &flight);
    ASSERT_EQ(rec.frames.size(), 1U);
    EXPECT_EQ(rec.frames.back(), flight.keyframes.front().settings);
    ASSERT_EQ(rec.statuses.size(), 1U);
    EXPECT_EQ(rec.statuses.back(), "Flight: Test flight (Esc stops)");
    EXPECT_EQ(rec.timer, std::vector<bool>{true});
    EXPECT_EQ(rec.finished, 0);
}

TEST(DemoPlayer, ATickShowsTheFrameForTheElapsedTime)
{
    Recorder     rec;
    DemoPlayer   player(rec.hooks());
    const Flight flight = testFlight();
    player.play(flight);
    rec.advance(milliseconds(500));
    player.tick();
    ASSERT_EQ(rec.frames.size(), 2U);
    EXPECT_EQ(rec.frames.back(), mandelbrotter::flightSettingsAt(flight, milliseconds(500)));
    EXPECT_DOUBLE_EQ(rec.frames.back().view.zoom, 4.0);  // halfway from 1x to 16x, exponentially
    EXPECT_EQ(rec.statuses.back(), "Flight: Test flight 25% (Esc stops)");

    rec.advance(milliseconds(1000));
    player.tick();
    EXPECT_EQ(rec.frames.back(), mandelbrotter::flightSettingsAt(flight, milliseconds(1500)));
    EXPECT_EQ(rec.statuses.back(), "Flight: Test flight 75% (Esc stops)");
    EXPECT_TRUE(player.playing());
}

TEST(DemoPlayer, AnUnchangedFrameIsNotAppliedAgain)
{
    Recorder     rec;
    DemoPlayer   player(rec.hooks());
    const Flight flight = testFlight();
    player.play(flight);
    player.tick();  // no time has passed: still the first keyframe
    EXPECT_EQ(rec.frames.size(), 1U);
    EXPECT_EQ(rec.statuses.back(), "Flight: Test flight 0% (Esc stops)");

    rec.advance(milliseconds(300));
    player.tick();
    player.tick();
    EXPECT_EQ(rec.frames.size(), 2U);
    EXPECT_EQ(rec.statuses.size(), 4U);  // the status is refreshed on every tick
}

TEST(DemoPlayer, ABusyCanvasDropsFramesButTheClockKeepsRunning)
{
    Recorder     rec;
    DemoPlayer   player(rec.hooks());
    const Flight flight = testFlight();
    player.play(flight);
    rec.ready = false;
    rec.advance(milliseconds(500));
    player.tick();
    EXPECT_EQ(rec.frames.size(), 1U);
    EXPECT_EQ(rec.statuses.size(), 1U);

    rec.ready = true;
    rec.advance(milliseconds(100));
    player.tick();
    ASSERT_EQ(rec.frames.size(), 2U);
    EXPECT_EQ(rec.frames.back(), mandelbrotter::flightSettingsAt(flight, milliseconds(600)));
}

TEST(DemoPlayer, TheFlightEndsOnItsLastKeyframe)
{
    Recorder     rec;
    DemoPlayer   player(rec.hooks());
    const Flight flight = testFlight();
    player.play(flight);
    rec.advance(milliseconds(1999));
    rec.ready = false;
    player.tick();
    EXPECT_TRUE(player.playing());

    rec.advance(milliseconds(1));
    player.tick();  // the end is shown even when the canvas is busy
    EXPECT_FALSE(player.playing());
    EXPECT_EQ(player.current(), nullptr);
    ASSERT_EQ(rec.frames.size(), 2U);
    EXPECT_EQ(rec.frames.back(), flight.keyframes.back().settings);
    EXPECT_EQ(rec.statuses.back(), "");
    EXPECT_EQ(rec.finished, 1);
    EXPECT_EQ(rec.timer, (std::vector<bool>{true, false}));

    rec.advance(milliseconds(1000));
    player.tick();  // a tick already queued by the toolkit
    EXPECT_EQ(rec.frames.size(), 2U);
    EXPECT_EQ(rec.finished, 1);
}

TEST(DemoPlayer, LateTicksJumpToTheEnd)
{
    Recorder     rec;
    DemoPlayer   player(rec.hooks());
    const Flight flight = testFlight();
    player.play(flight);
    rec.advance(std::chrono::seconds(60));
    player.tick();
    EXPECT_FALSE(player.playing());
    EXPECT_EQ(rec.frames.back(), flight.keyframes.back().settings);
}

TEST(DemoPlayer, StopKeepsTheViewAndReports)
{
    Recorder     rec;
    DemoPlayer   player(rec.hooks());
    const Flight flight = testFlight();
    player.play(flight);
    rec.advance(milliseconds(500));
    player.tick();
    player.stop();
    EXPECT_FALSE(player.playing());
    EXPECT_EQ(rec.frames.size(), 2U);  // nothing new applied
    EXPECT_EQ(rec.statuses.back(), "");
    EXPECT_EQ(rec.finished, 1);
    EXPECT_EQ(rec.timer, (std::vector<bool>{true, false}));
}

TEST(DemoPlayer, StopWhileIdleDoesNothing)
{
    Recorder   rec;
    DemoPlayer player(rec.hooks());
    player.stop();
    player.tick();
    EXPECT_TRUE(rec.frames.empty());
    EXPECT_TRUE(rec.statuses.empty());
    EXPECT_TRUE(rec.timer.empty());
    EXPECT_EQ(rec.finished, 0);
}

TEST(DemoPlayer, AFlightWithoutKeyframesIsIgnored)
{
    Recorder     rec;
    DemoPlayer   player(rec.hooks());
    const Flight flight = testFlight();
    const Flight empty{.id = "empty", .title = "Empty", .description = "", .keyframes = {}};
    player.play(empty);
    EXPECT_FALSE(player.playing());
    EXPECT_TRUE(rec.frames.empty());
    EXPECT_TRUE(rec.timer.empty());

    player.play(flight);
    player.play(empty);  // does not stop the flight that plays
    EXPECT_EQ(player.current(), &flight);
}

TEST(DemoPlayer, PlayingAgainRestartsFromTheBeginning)
{
    Recorder     rec;
    DemoPlayer   player(rec.hooks());
    const Flight flight = testFlight();
    player.play(flight);
    rec.advance(milliseconds(1500));
    player.tick();
    const Flight other = testFlight();
    player.play(other);
    EXPECT_EQ(player.current(), &other);
    EXPECT_EQ(rec.finished, 1);  // the first flight was stopped
    EXPECT_EQ(rec.timer, (std::vector<bool>{true, false, true}));
    EXPECT_EQ(rec.frames.back(), other.keyframes.front().settings);
    EXPECT_EQ(rec.statuses.back(), "Flight: Test flight (Esc stops)");

    rec.advance(milliseconds(500));
    player.tick();
    EXPECT_EQ(rec.statuses.back(),
              "Flight: Test flight 25% (Esc stops)");  // timed from the restart
}

TEST(DemoPlayer, HooksBeyondTheEssentialOnesAreOptional)
{
    int          applied = 0;
    DemoPlayer   player({.applyFrame      = [&](const RenderSettings&) { ++applied; },
                         .canvasReady     = {},
                         .showStatus      = [](std::string_view) { },
                         .finished        = {},
                         .setTimerRunning = {},
                         .now             = {}});
    const Flight flight = testFlight();
    player.play(flight);
    player.tick();
    player.stop();
    EXPECT_EQ(applied, 1);
    EXPECT_FALSE(player.playing());
    EXPECT_EQ(mandelbrotter::app::kDemoTick, milliseconds(33));
}

}  // namespace
