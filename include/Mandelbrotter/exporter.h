#pragma once

#include <optional>
#include <stop_token>

#include "Mandelbrotter/ProgressiveRenderer.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"

namespace mandelbrotter
{

inline constexpr int kMaxSupersample     = 4;
inline constexpr int kMaxExportDimension = 16384;

struct ExportOptions
{
    PixelSize size{1920, 1080};
    int       supersample{1};  ///< 1, 2 or 4: samples per pixel along each axis

    bool operator==(const ExportOptions&) const = default;
};

/// Renders the view at options.size, supersampled and box-filtered down. Bounded memory: the
/// supersampled image is rendered in horizontal bands. Returns nullopt if `stop` was requested.
/// Throws std::invalid_argument for an empty or oversized image.
[[nodiscard]] std::optional<RgbImage> renderForExport(const RenderSettings&   settings,
                                                      const ExportOptions&    options,
                                                      const std::stop_token&  stop     = {},
                                                      const ProgressCallback& progress = {});

}  // namespace mandelbrotter
