#pragma once

#include <optional>

#include <wx/bitmap.h>
#include <wx/timer.h>
#include <wx/window.h>

#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::gui
{

/// A small live thumbnail of the Julia set for a seed, re-rendered (throttled) as the seed changes.
class JuliaPreview : public wxWindow
{
public:
    explicit JuliaPreview(wxWindow* parent);

    /// The family and exponent to preview (the Julia flag and seed are ignored).
    void setFractal(const FractalSpec& spec);
    void setColoring(const ColoringSettings& coloring);
    /// nullopt clears the preview.
    void setSeed(std::optional<Complex> seed);

private:
    void onPaint(wxPaintEvent& event);
    void onSize(wxSizeEvent& event);
    void onTimer(wxTimerEvent& event);
    void schedule();
    void render();

    FractalSpec            m_spec;
    ColoringSettings       m_coloring;
    std::optional<Complex> m_seed;
    wxTimer                m_timer;
    wxBitmap               m_bitmap;
};

}  // namespace mandelbrotter::gui
