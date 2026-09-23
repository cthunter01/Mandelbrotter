#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <wx/event.h>
#include <wx/gdicmn.h>

#include "Mandelbrotter/RenderSettings.h"

namespace mandelbrotter::gui
{

class MainFrame;
class TourCard;

/// Help > Take a tour: eleven steps that each explain one part of the window and perform the
/// action they describe. Every step sets absolute state, so Back simply re-runs the previous step.
/// Closing (or finishing) puts back the view, the orbit overlay and the bookmark list.
class GuidedTour : public wxEvtHandler
{
public:
    explicit GuidedTour(MainFrame& frame);
    ~GuidedTour() override;
    GuidedTour(const GuidedTour&)            = delete;
    GuidedTour& operator=(const GuidedTour&) = delete;
    GuidedTour(GuidedTour&&)                 = delete;
    GuidedTour& operator=(GuidedTour&&)      = delete;

    void start();
    /// Ends the tour and restores what the user had. No effect when not running.
    void               stop();
    [[nodiscard]] bool running() const noexcept { return m_running; }

    void                      next();
    void                      back();
    void                      showStep(std::size_t index);
    [[nodiscard]] std::size_t stepCount() const noexcept { return m_steps.size(); }

private:
    enum class Target : std::uint8_t
    {
        CANVAS,
        FRACTAL,
        ITERATIONS,
        COLOURING,
        OVERLAY,
        BOOKMARKS,
    };
    struct Step
    {
        std::string           title;
        std::string           text;
        std::string           helpPage;  ///< "Learn more" target; empty hides the button
        Target                target;
        std::function<void()> perform;
    };
    struct Baseline
    {
        RenderSettings settings;
        bool           showOrbit{};
    };

    [[nodiscard]] std::vector<Step> buildSteps();
    void                            leaveCurrentStep();
    void                            highlight(Target target);
    void                            clearHighlights();
    [[nodiscard]] wxRect            anchorFor(Target target) const;
    void                            restoreBaseline();
    void                            onFrameResized(wxSizeEvent& event);

    MainFrame&        m_frame;
    std::vector<Step> m_steps;
    std::size_t       m_index{0};
    TourCard*         m_card{nullptr};
    Baseline          m_baseline;
    bool              m_running{false};
};

}  // namespace mandelbrotter::gui
