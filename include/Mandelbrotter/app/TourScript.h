#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "Mandelbrotter/RenderSettings.h"

namespace mandelbrotter::app
{

class AppController;

/// The parts of the window a tour step points at.
enum class TourTarget : std::uint8_t
{
    CANVAS,
    FRACTAL,
    ITERATIONS,
    COLORING,
    OVERLAY,
    BOOKMARKS,
};

struct TourStep
{
    std::string                         title;
    std::string                         text;
    std::string                         helpPage;  ///< "Learn more" target; empty hides the button
    TourTarget                          anchor{TourTarget::CANVAS};  ///< where the card goes
    std::optional<TourTarget>           highlight;  ///< the accent ring; nullopt: none
    std::function<void(AppController&)> perform;    ///< what the step shows
};

/// Help > Take a tour: eleven steps that each explain one part of the window and perform the
/// action they describe. Every step sets absolute state (the view, the orbit overlay), so Back
/// simply re-runs the previous step. Closing (or finishing) puts back the view, the orbit overlay
/// and the bookmark list the user had.
///
/// The toolkit shows the step's card beside its anchor and draws the highlight ring.
class TourScript
{
public:
    struct Hooks
    {
        /// Show (creating it the first time) the card for step `index` of `count`.
        std::function<void(const TourStep&, std::size_t index, std::size_t count)> showCard;
        std::function<void()>                                                      hideCard;
        std::function<void(std::optional<TourTarget>)>                             highlight;
    };

    /// `app` must outlive the tour.
    TourScript(AppController& app, Hooks hooks);
    TourScript(const TourScript&)            = delete;
    TourScript& operator=(const TourScript&) = delete;
    TourScript(TourScript&&)                 = delete;
    TourScript& operator=(TourScript&&)      = delete;
    ~TourScript()                            = default;

    /// Remembers what the user has and shows the first step; while running, back to the first step.
    void start();
    /// Ends the tour and restores what the user had. No effect when not running.
    void               stop();
    [[nodiscard]] bool running() const noexcept { return m_running; }

    /// The next step; after the last one the tour ends.
    void next();
    void back();
    /// Ignored when not running or out of range.
    void showStep(std::size_t index);

    [[nodiscard]] std::size_t                  stepCount() const noexcept { return m_steps.size(); }
    [[nodiscard]] std::size_t                  currentStep() const noexcept { return m_index; }
    [[nodiscard]] const std::vector<TourStep>& steps() const noexcept { return m_steps; }

private:
    struct Baseline
    {
        RenderSettings settings;
        bool           showOrbit{};
    };

    [[nodiscard]] std::vector<TourStep> buildSteps();
    void                                leaveCurrentStep();
    void                                restoreBaseline();

    AppController&        m_app;
    Hooks                 m_hooks;
    std::vector<TourStep> m_steps;
    std::size_t           m_index{0};
    Baseline              m_baseline;
    bool                  m_running{false};
};

}  // namespace mandelbrotter::app
