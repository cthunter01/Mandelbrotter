#include "gui/GuidedTour.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/flights.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "gui/FractalCanvas.h"
#include "gui/MainFrame.h"
#include "gui/SidePanel.h"
#include "gui/TourCard.h"

namespace mandelbrotter::gui
{

namespace
{

constexpr Complex kSeahorseValley{-0.7436, 0.1318};
constexpr Complex kJuliaSeed{-0.8, 0.156};
constexpr Complex kOrbitPoint{0.285, 0.01};
constexpr double  kDeepZoom = 1e10;

RenderSettings at(RenderSettings settings, Complex center, double zoom)
{
    settings.view.zoom   = clampZoom(zoom);
    settings.view.center = BigComplex::fromComplex(center, fractionBitsFor(settings.view.zoom));
    return settings;
}

RenderSettings mandelbrotDefault()
{
    RenderSettings settings;
    settings.view = defaultView(settings.fractal);
    return settings;
}

RenderSettings seahorse(const char* palette = "electric")
{
    RenderSettings settings   = at(mandelbrotDefault(), kSeahorseValley, 5000.0);
    settings.coloring.palette = palette;
    return settings;
}

}  // namespace

GuidedTour::GuidedTour(MainFrame& frame) : m_frame(frame), m_steps(buildSteps()) { }

GuidedTour::~GuidedTour() = default;

std::vector<GuidedTour::Step> GuidedTour::buildSteps()
{
    // Every step sets absolute state (not a change relative to the previous one), so going back
    // and forth always shows the same thing.
    return {
        {.title = "Welcome to Mandelbrotter",
         .text  = "This tour walks through the window in eleven short steps and performs each "
                  "action as it goes. Use Next and Back at your own pace; Close (or Esc) ends the "
                  "tour and puts back the view you had.\n\nThe big area is the picture, the panel "
                  "on the right holds every setting, and the status bar along the bottom reports "
                  "where you are.",
         .helpPage = "getting-started.html",
         .target   = Target::CANVAS,
         .perform  = [this] { m_frame.applySettings(mandelbrotDefault()); }},
        {.title = "Zooming and panning",
         .text  = "We have jumped into Seahorse Valley at zoom 5000. The mouse wheel zooms at the "
                  "pointer, dragging pans, dragging with the right button (or Shift) zooms to a "
                  "rectangle, and a right-click zooms out. The arrow keys, + and -, and Home work "
                  "too. Try a few wheel notches now; the tour will not mind.",
         .helpPage = "navigating.html",
         .target   = Target::CANVAS,
         .perform  = [this] { m_frame.applySettings(seahorse()); }},
        {.title    = "Fractal families",
         .text     = "The Family list switches between the Mandelbrot set, the Burning Ship (shown "
                     "now) and the Tricorn; Exponent n turns z^2 + c into z^n + c. Changing the "
                     "family resets the view to show the whole set; changing n keeps it.",
         .helpPage = "fractals.html",
         .target   = Target::FRACTAL,
         .perform =
             [this] {
                 RenderSettings settings;
                 settings.fractal.family = FractalFamily::BURNING_SHIP;
                 settings.view           = defaultView(settings.fractal);
                 m_frame.applySettings(settings);
             }},
        {.title = "Julia sets",
         .text  = "Tick Julia set to draw the Julia set of the constant c typed into the seed "
                  "fields; this one is c = -0.8 + 0.156i. Pick seed from canvas lets you click a "
                  "point of the Mandelbrot set instead, and the small preview follows the mouse "
                  "so you can see a Julia set before you commit to it.",
         .helpPage = "julia.html",
         .target   = Target::FRACTAL,
         .perform =
             [this] {
                 RenderSettings settings;
                 settings.fractal.julia = true;
                 settings.fractal.seed  = kJuliaSeed;
                 settings.view          = defaultView(settings.fractal);
                 m_frame.applySettings(settings);
                 m_frame.panel().setPreviewSeed(kJuliaSeed);
             }},
        {.title = "Iterations",
         .text  = "Every point is iterated until it escapes or the limit is reached; points that "
                  "never escape are painted black. Maximum sets the limit, and with Auto ticked it "
                  "grows as you zoom in (the effective limit is shown below). Too few iterations "
                  "blur the fine detail; too many just cost time.",
         .helpPage = "iterations.html",
         .target   = Target::ITERATIONS,
         .perform  = [this] { m_frame.applySettings(seahorse()); }},
        {.title = "Colouring",
         .text  = "The palette maps how fast a point escapes onto a colour cycle. Density sets how "
                  "many iterations one cycle spans and Offset shifts the cycle. Changing any of "
                  "them recolours the picture at once, without recomputing it; we just switched "
                  "to the fire palette with a denser cycle.",
         .helpPage = "colouring.html",
         .target   = Target::COLOURING,
         .perform =
             [this] {
                 RenderSettings settings   = seahorse("fire");
                 settings.coloring.density = 32.0;
                 settings.coloring.offset  = 0.25;
                 m_frame.applySettings(settings);
             }},
        {.title    = "The orbit overlay",
         .text     = "With the overlay on, the path of the point under the mouse is drawn as it is "
                     "iterated: the red circle is where it starts and the white line is where it "
                     "goes. Points inside the set stay trapped; points outside fly away. Move the "
                     "mouse over the edge of the set to watch.",
         .helpPage = "orbit.html",
         .target   = Target::OVERLAY,
         .perform =
             [this] {
                 m_frame.applySettings(mandelbrotDefault());
                 m_frame.setShowOrbit(true);
                 m_frame.canvas().showOrbitAt(kOrbitPoint);
             }},
        {.title    = "Bookmarks",
         .text     = "Add... saves the current view, fractal and colours under a name; Load (or a "
                     "double-click) returns to it and Delete removes it. Bookmarks are kept in a "
                     "small JSON file in your user data folder. The entry \"Tour example\" was "
                     "added for this step and will disappear when the tour ends.",
         .helpPage = "bookmarks.html",
         .target   = Target::BOOKMARKS,
         .perform =
             [this] {
                 m_frame.applySettings(seahorse());
                 m_frame.addTemporaryBookmark("Tour example");
             }},
        {.title = "Saving a picture",
         .text  = "File > Save image as PNG renders the view again at any size you like, with "
                  "optional anti-aliasing, and writes a PNG file; File > Copy image puts what is "
                  "on screen onto the clipboard. The dialog is open now; it closes when you move "
                  "on.",
         .helpPage = "exporting.html",
         .target   = Target::CANVAS,
         .perform  = [this] { m_frame.showExportDialog(); }},
        {.title = "Deep zoom",
         .text  = "This is the same spot at zoom 1e10, far beyond where ordinary double "
                  "precision could tell neighbouring pixels apart. Above 1e8 the status bar says "
                  "\"(deep)\": the centre is kept with as many digits as the zoom needs and every "
                  "pixel is computed as a small difference from it. You can go on to 1e300.",
         .helpPage = "deep-zoom.html",
         .target   = Target::CANVAS,
         .perform =
             [this] {
                 // The seahorse dive's destination has structure at every depth.
                 RenderSettings settings = findFlight("seahorse-dive")->keyframes.back().settings;
                 settings.view.zoom      = kDeepZoom;
                 settings.view.center =
                     settings.view.center.withFractionBits(fractionBitsFor(kDeepZoom));
                 m_frame.applySettings(settings);
             }},
        {.title = "That is the tour",
         .text  = "Your view, overlay and bookmarks are back as they were. Help > Contents holds "
                  "the full guide (F1 opens the page for whatever has the focus), Help > Demos "
                  "plays animated dives into famous places, and Help > Back to where I was "
                  "returns to the view from before any demo.",
         .helpPage = "index.html",
         .target   = Target::CANVAS,
         .perform =
             [this] {
                 restoreBaseline();
                 clearHighlights();
             }},
    };
}

void GuidedTour::start()
{
    if (m_running)
    {
        showStep(0);
        return;
    }
    m_running       = true;
    m_baseline      = {.settings = m_frame.settings(), .showOrbit = m_frame.canvas().showOrbit()};
    m_card          = new TourCard(&m_frame);
    m_card->onBack  = [this] { back(); };
    m_card->onNext  = [this] { next(); };
    m_card->onClose = [this] { stop(); };
    m_card->onLearnMore = [this] {
        if (m_index < m_steps.size())
        {
            m_frame.showHelpPage(m_steps[m_index].helpPage);
        }
    };
    m_frame.Bind(wxEVT_SIZE, &GuidedTour::onFrameResized, this);
    showStep(0);
}

void GuidedTour::stop()
{
    if (!m_running)
    {
        return;
    }
    m_running = false;
    m_frame.Unbind(wxEVT_SIZE, &GuidedTour::onFrameResized, this);
    leaveCurrentStep();
    restoreBaseline();
    clearHighlights();
    if (m_card != nullptr)
    {
        m_card->Destroy();
        m_card = nullptr;
    }
}

void GuidedTour::next()
{
    if (m_index + 1 >= m_steps.size())
    {
        stop();
        return;
    }
    showStep(m_index + 1);
}

void GuidedTour::back()
{
    if (m_index > 0)
    {
        showStep(m_index - 1);
    }
}

void GuidedTour::showStep(std::size_t index)
{
    if (!m_running || index >= m_steps.size())
    {
        return;
    }
    leaveCurrentStep();
    m_index          = index;
    const Step& step = m_steps[index];
    step.perform();
    highlight(step.target);
    m_card->setStep(step.title, step.text, index, m_steps.size(), !step.helpPage.empty());
    m_card->placeNear(anchorFor(step.target));
}

void GuidedTour::leaveCurrentStep()
{
    m_frame.closeExportDialog();
    m_frame.canvas().clearPinnedOrbit();
}

void GuidedTour::highlight(Target target)
{
    m_frame.canvas().setHighlighted(target == Target::CANVAS);
    std::optional<SidePanel::Section> section;
    switch (target)
    {
        case Target::FRACTAL:
            section = SidePanel::Section::FRACTAL;
            break;
        case Target::ITERATIONS:
            section = SidePanel::Section::ITERATIONS;
            break;
        case Target::COLOURING:
            section = SidePanel::Section::COLOURING;
            break;
        case Target::OVERLAY:
            section = SidePanel::Section::OVERLAY;
            break;
        case Target::BOOKMARKS:
            section = SidePanel::Section::BOOKMARKS;
            break;
        case Target::CANVAS:
            break;
    }
    if (section)
    {
        m_frame.setSidePanelShown(true);
        m_frame.panel().scrollToSection(*section);
    }
    m_frame.panel().setHighlightedSection(section);
}

void GuidedTour::clearHighlights()
{
    m_frame.canvas().setHighlighted(false);
    m_frame.panel().setHighlightedSection(std::nullopt);
}

wxRect GuidedTour::anchorFor(Target target) const
{
    if (target == Target::CANVAS)
    {
        return m_frame.canvas().GetRect();
    }
    SidePanel::Section section = SidePanel::Section::FRACTAL;
    switch (target)
    {
        case Target::ITERATIONS:
            section = SidePanel::Section::ITERATIONS;
            break;
        case Target::COLOURING:
            section = SidePanel::Section::COLOURING;
            break;
        case Target::OVERLAY:
            section = SidePanel::Section::OVERLAY;
            break;
        case Target::BOOKMARKS:
            section = SidePanel::Section::BOOKMARKS;
            break;
        case Target::FRACTAL:
        case Target::CANVAS:
            break;
    }
    const wxRect  inPanel = m_frame.panel().sectionRect(section);
    const wxPoint origin =
        m_frame.ScreenToClient(m_frame.panel().ClientToScreen(inPanel.GetPosition()));
    return {origin, inPanel.GetSize()};
}

void GuidedTour::restoreBaseline()
{
    m_frame.removeTemporaryBookmark();
    m_frame.applySettings(m_baseline.settings);
    m_frame.setShowOrbit(m_baseline.showOrbit);
}

void GuidedTour::onFrameResized(wxSizeEvent& event)
{
    event.Skip();
    if (m_running && m_card != nullptr && m_index < m_steps.size())
    {
        // The sizer lays the canvas and panel out after this event; place the card once it has.
        m_frame.CallAfter([this] {
            if (m_running && m_card != nullptr)
            {
                m_card->placeNear(anchorFor(m_steps[m_index].target));
            }
        });
    }
}

}  // namespace mandelbrotter::gui
