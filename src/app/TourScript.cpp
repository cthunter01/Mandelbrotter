#include "Mandelbrotter/app/TourScript.h"

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/panel_model.h"
#include "Mandelbrotter/app/scenes.h"

namespace mandelbrotter::app
{

std::optional<PanelSection> panelSectionFor(TourTarget target) noexcept
{
    switch (target)
    {
        case TourTarget::FRACTAL:
            return PanelSection::FRACTAL;
        case TourTarget::ITERATIONS:
            return PanelSection::ITERATIONS;
        case TourTarget::COLORING:
            return PanelSection::COLORING;
        case TourTarget::OVERLAY:
            return PanelSection::OVERLAY;
        case TourTarget::BOOKMARKS:
            return PanelSection::BOOKMARKS;
        case TourTarget::CANVAS:
            break;
    }
    return std::nullopt;
}

TourScript::TourScript(AppController& app, Hooks hooks)
  : m_app(app), m_hooks(std::move(hooks)), m_steps(buildSteps())
{
}

std::vector<TourStep> TourScript::buildSteps()
{
    // Every step sets absolute state (not a change relative to the previous one), so going back
    // and forth always shows the same thing. Only the orbit step turns the overlay on.
    return {
        {.title = "Welcome to Mandelbrotter",
         .text  = "This tour walks through the window in eleven short steps and performs each "
                  "action as it goes. Use Next and Back at your own pace; Close (or Esc) ends the "
                  "tour and puts back the view you had.\n\nThe big area is the picture, the panel "
                  "on the right holds every setting, and the status bar along the bottom reports "
                  "where you are.",
         .helpPage  = "getting-started.html",
         .anchor    = TourTarget::CANVAS,
         .highlight = TourTarget::CANVAS,
         .perform =
             [](AppController& app) {
                 app.applySettings(mandelbrotDefault());
                 app.setShowOrbit(false);
             }},
        {.title = "Zooming and panning",
         .text  = "We have jumped into Seahorse Valley at zoom 5000. The mouse wheel zooms at the "
                  "pointer, dragging pans, dragging with the right button (or Shift) zooms to a "
                  "rectangle, and a right-click zooms out. The arrow keys, + and -, and Home work "
                  "too. Try a few wheel notches now; the tour will not mind.",
         .helpPage  = "navigating.html",
         .anchor    = TourTarget::CANVAS,
         .highlight = TourTarget::CANVAS,
         .perform =
             [](AppController& app) {
                 app.applySettings(seahorse());
                 app.setShowOrbit(false);
             }},
        {.title    = "Fractal families",
         .text     = "The Family list switches between the Mandelbrot set, the Burning Ship (shown "
                     "now) and the Tricorn; Exponent n turns z^2 + c into z^n + c. Changing the "
                     "family resets the view to show the whole set; changing n keeps it.",
         .helpPage = "fractals.html",
         .anchor   = TourTarget::FRACTAL,
         .highlight = TourTarget::FRACTAL,
         .perform =
             [](AppController& app) {
                 app.applySettings(burningShipDefault());
                 app.setShowOrbit(false);
             }},
        {.title = "Julia sets",
         .text  = "Tick Julia set to draw the Julia set of the constant c typed into the seed "
                  "fields; this one is c = -0.8 + 0.156i. Pick seed from canvas lets you click a "
                  "point of the Mandelbrot set instead, and the small preview follows the mouse "
                  "so you can see a Julia set before you commit to it.",
         .helpPage  = "julia.html",
         .anchor    = TourTarget::FRACTAL,
         .highlight = TourTarget::FRACTAL,
         .perform =
             [](AppController& app) {
                 app.applySettings(juliaExample());
                 app.setPreviewSeed(kJuliaSeed);
                 app.setShowOrbit(false);
             }},
        {.title = "Iterations",
         .text  = "Every point is iterated until it escapes or the limit is reached; points that "
                  "never escape are painted black. Maximum sets the limit, and with Auto ticked it "
                  "grows as you zoom in (the effective limit is shown below). Too few iterations "
                  "blur the fine detail; too many just cost time.",
         .helpPage  = "iterations.html",
         .anchor    = TourTarget::ITERATIONS,
         .highlight = TourTarget::ITERATIONS,
         .perform =
             [](AppController& app) {
                 app.applySettings(seahorse());
                 app.setShowOrbit(false);
             }},
        {.title = "Coloring",
         .text  = "The palette maps how fast a point escapes onto a color cycle. Density sets how "
                  "many iterations one cycle spans and Offset shifts the cycle. Changing any of "
                  "them recolors the picture at once, without recomputing it; we just switched "
                  "to the fire palette with a denser cycle.",
         .helpPage  = "coloring.html",
         .anchor    = TourTarget::COLORING,
         .highlight = TourTarget::COLORING,
         .perform =
             [](AppController& app) {
                 app.applySettings(seahorseFire());
                 app.setShowOrbit(false);
             }},
        {.title    = "The orbit overlay",
         .text     = "With the overlay on, the path of the point under the mouse is drawn as it is "
                     "iterated: the red circle is where it starts and the white line is where it "
                     "goes. Points inside the set stay trapped; points outside fly away. Move the "
                     "mouse over the edge of the set to watch.",
         .helpPage = "orbit.html",
         .anchor   = TourTarget::OVERLAY,
         .highlight = TourTarget::OVERLAY,
         .perform =
             [](AppController& app) {
                 app.applySettings(mandelbrotDefault());
                 app.setShowOrbit(true);
                 app.canvas().showOrbitAt(kOrbitPoint);
             }},
        {.title     = "Bookmarks",
         .text      = "Add... saves the current view, fractal and colors under a name; Load (or a "
                      "double-click) returns to it and Delete removes it. Bookmarks are kept in a "
                      "small JSON file in your user data folder. The entry \"Tour example\" was "
                      "added for this step and will disappear when the tour ends.",
         .helpPage  = "bookmarks.html",
         .anchor    = TourTarget::BOOKMARKS,
         .highlight = TourTarget::BOOKMARKS,
         .perform =
             [](AppController& app) {
                 app.applySettings(seahorse());
                 app.setShowOrbit(false);
                 app.addTemporaryBookmark("Tour example");
             }},
        {.title = "Saving a picture",
         .text  = "File > Save image as PNG renders the view again at any size you like, with "
                  "optional anti-aliasing, and writes a PNG file; File > Copy image puts what is "
                  "on screen onto the clipboard. The dialog is open now; it closes when you move "
                  "on.",
         .helpPage  = "exporting.html",
         .anchor    = TourTarget::CANVAS,
         .highlight = TourTarget::CANVAS,
         .perform =
             [](AppController& app) {
                 app.setShowOrbit(false);
                 app.showExportDialog();
             }},
        {.title = "Deep zoom",
         .text  = "This is the same spot at zoom 1e10, far beyond where ordinary double "
                  "precision could tell neighbouring pixels apart. Above 1e8 the status bar says "
                  "\"(deep)\": the center is kept with as many digits as the zoom needs and every "
                  "pixel is computed as a small difference from it. You can go on to 1e300.",
         .helpPage  = "deep-zoom.html",
         .anchor    = TourTarget::CANVAS,
         .highlight = TourTarget::CANVAS,
         .perform =
             [](AppController& app) {
                 // The seahorse dive's destination has structure at every depth.
                 app.applySettings(deepSeahorse(kTourDeepZoom));
                 app.setShowOrbit(false);
             }},
        {.title = "That is the tour",
         .text  = "Your view, overlay and bookmarks are back as they were. Help > Contents holds "
                  "the full guide (F1 opens the page for whatever has the focus), Help > Demos "
                  "plays animated dives into famous places, and Help > Back to where I was "
                  "returns to the view from before any demo.",
         .helpPage  = "index.html",
         .anchor    = TourTarget::CANVAS,
         .highlight = std::nullopt,
         .perform   = [this](AppController& /*app*/) { restoreBaseline(); }},
    };
}

void TourScript::start()
{
    if (m_running)
    {
        showStep(0);
        return;
    }
    m_running  = true;
    m_baseline = {.settings = m_app.settings(), .showOrbit = m_app.showOrbit()};
    showStep(0);
}

void TourScript::stop()
{
    if (!m_running)
    {
        return;
    }
    m_running = false;
    leaveCurrentStep();
    restoreBaseline();
    if (m_hooks.highlight)
    {
        m_hooks.highlight(std::nullopt);
    }
    if (m_hooks.hideCard)
    {
        m_hooks.hideCard();
    }
}

void TourScript::next()
{
    if (m_index + 1 >= m_steps.size())
    {
        stop();
        return;
    }
    showStep(m_index + 1);
}

void TourScript::back()
{
    if (m_index > 0)
    {
        showStep(m_index - 1);
    }
}

void TourScript::showStep(std::size_t index)
{
    if (!m_running || index >= m_steps.size())
    {
        return;
    }
    leaveCurrentStep();
    m_index              = index;
    const TourStep& step = m_steps[index];
    step.perform(m_app);
    if (m_hooks.highlight)
    {
        m_hooks.highlight(step.highlight);
    }
    if (m_hooks.showCard)
    {
        m_hooks.showCard(step, index, m_steps.size());
    }
}

void TourScript::leaveCurrentStep()
{
    m_app.closeExportDialog();
    m_app.canvas().clearPinnedOrbit();
}

void TourScript::restoreBaseline()
{
    m_app.removeTemporaryBookmark();
    m_app.applySettings(m_baseline.settings);
    m_app.setShowOrbit(m_baseline.showOrbit);
}

}  // namespace mandelbrotter::app
