#include "gui/JuliaPreview.h"

#include <algorithm>
#include <cmath>

#include <wx/dcbuffer.h>

#include "Mandelbrotter/ProgressiveRenderer.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/image.h"
#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

namespace
{

constexpr int kThrottleMs        = 50;
constexpr int kPreviewIterations = 128;

}  // namespace

JuliaPreview::JuliaPreview(wxWindow* parent)
  : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
             wxBORDER_SUNKEN | wxFULL_REPAINT_ON_RESIZE),
    m_timer(this)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(FromDIP(wxSize(200, 150)));
    Bind(wxEVT_PAINT, &JuliaPreview::onPaint, this);
    Bind(wxEVT_SIZE, &JuliaPreview::onSize, this);
    Bind(wxEVT_TIMER, &JuliaPreview::onTimer, this, m_timer.GetId());
}

void JuliaPreview::setFractal(const FractalSpec& spec)
{
    if (m_spec.family == spec.family && m_spec.exponent == spec.exponent)
    {
        return;
    }
    m_spec.family   = spec.family;
    m_spec.exponent = spec.exponent;
    schedule();
}

void JuliaPreview::setColoring(const ColoringSettings& coloring)
{
    if (m_coloring == coloring)
    {
        return;
    }
    m_coloring = coloring;
    schedule();
}

void JuliaPreview::setSeed(std::optional<Complex> seed)
{
    if (m_seed == seed)
    {
        return;
    }
    m_seed = seed;
    schedule();
}

void JuliaPreview::schedule()
{
    if (!m_timer.IsRunning())
    {
        m_timer.StartOnce(kThrottleMs);
    }
}

void JuliaPreview::onTimer(wxTimerEvent& /*event*/)
{
    render();
}

void JuliaPreview::onSize(wxSizeEvent& event)
{
    schedule();
    event.Skip();
}

void JuliaPreview::render()
{
    const wxSize client = GetClientSize();
    if (!m_seed || client.x <= 0 || client.y <= 0)
    {
        m_bitmap = wxBitmap();
        Refresh(false);
        return;
    }
    const double    scale = GetContentScaleFactor();
    const PixelSize size{std::max(1, static_cast<int>(std::lround(client.x * scale))),
                         std::max(1, static_cast<int>(std::lround(client.y * scale)))};

    RenderSettings settings;
    settings.fractal = {
        .family = m_spec.family, .exponent = m_spec.exponent, .julia = true, .seed = *m_seed};
    settings.view           = defaultView(settings.fractal);
    settings.maxIterations  = kPreviewIterations;
    settings.autoIterations = false;
    settings.coloring       = m_coloring;

    const auto buffer = renderSync(settings, size);
    if (!buffer)
    {
        return;
    }
    const RgbImage image = colorized(*buffer, paletteOrDefault(m_coloring.palette), m_coloring);
    m_bitmap             = wxBitmap(toWxImage(image), -1, scale);
    Refresh(false);
}

void JuliaPreview::onPaint(wxPaintEvent& /*event*/)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(*wxBLACK_BRUSH);
    dc.Clear();
    if (m_bitmap.IsOk())
    {
        dc.DrawBitmap(m_bitmap, 0, 0, false);
    }
}

}  // namespace mandelbrotter::gui
