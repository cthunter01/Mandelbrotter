#include "Mandelbrotter/exporter.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <stop_token>

#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"
#include "Mandelbrotter/palette.h"
#include "Mandelbrotter/render_settings.h"
#include "Mandelbrotter/renderer.h"

namespace mandelbrotter
{

namespace
{

/// Supersampled rows per band. A multiple of every allowed supersample factor.
constexpr int kBandRows = 512;

}  // namespace

std::optional<RgbImage> renderForExport(const RenderSettings& settings,
                                        const ExportOptions& options, const std::stop_token& stop,
                                        const ProgressCallback& progress)
{
    if (options.size.empty())
    {
        throw std::invalid_argument("export size must be positive");
    }
    if (options.size.width > kMaxExportDimension || options.size.height > kMaxExportDimension)
    {
        throw std::invalid_argument("export size is too large");
    }
    const int       factor = std::clamp(options.supersample, 1, kMaxSupersample);
    const PixelSize full{options.size.width * factor, options.size.height * factor};
    const Palette&  palette = paletteOrDefault(settings.coloring.palette);

    const int bandCount  = (full.height + kBandRows - 1) / kBandRows;
    int       totalTiles = 0;
    for (int band = 0; band < bandCount; ++band)
    {
        const int rows = std::min(kBandRows, full.height - (band * kBandRows));
        totalTiles += static_cast<int>(
            tileGrid({0, band * kBandRows, full.width, rows}, kDefaultTileSize).size());
    }

    RgbImage out(options.size.width, options.size.height);
    int      tilesBefore = 0;
    for (int band = 0; band < bandCount; ++band)
    {
        const int              y0   = band * kBandRows;
        const int              rows = std::min(kBandRows, full.height - y0);
        const PixelRect        region{0, y0, full.width, rows};
        const ProgressCallback bandProgress =
            progress ? [&](int done, int) { progress(tilesBefore + done, totalTiles); }
                     : ProgressCallback{};
        const std::optional<IterationBuffer> buffer =
            renderSync(settings, full, 0, region, stop, bandProgress);
        if (!buffer)
        {
            return std::nullopt;
        }
        tilesBefore += static_cast<int>(tileGrid(region, kDefaultTileSize).size());
        const RgbImage bandImage =
            downsample(colorized(*buffer, palette, settings.coloring), factor);
        const int         outY  = y0 / factor;
        const std::size_t bytes = static_cast<std::size_t>(bandImage.width) *
                                  static_cast<std::size_t>(bandImage.height) * 3;
        std::memcpy(&out.pixels[out.offset(0, outY)], bandImage.pixels.data(), bytes);
    }
    return out;
}

}  // namespace mandelbrotter
