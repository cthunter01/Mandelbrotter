#pragma once

#include <string_view>
#include <vector>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::app
{

/// The places the guided tour and the screenshot mode show.
inline constexpr Complex kSeahorseValley{-0.7436, 0.1318};
inline constexpr Complex kJuliaSeed{-0.8, 0.156};
/// A point whose orbit shows well in the overlay.
inline constexpr Complex kOrbitPoint{0.285, 0.01};
/// The tour's deep-zoom step and the screenshot of the "(deep)" status bar.
inline constexpr double kTourDeepZoom       = 1e10;
inline constexpr double kScreenshotDeepZoom = 1e12;

/// `base` centred on `center` at `zoom` (clamped), with the precision that zoom calls for.
[[nodiscard]] RenderSettings viewAt(RenderSettings base, Complex center, double zoom);
/// Default settings showing the whole Mandelbrot set.
[[nodiscard]] RenderSettings mandelbrotDefault();
/// The whole Burning Ship.
[[nodiscard]] RenderSettings burningShipDefault();
/// The Julia set of kJuliaSeed, whole.
[[nodiscard]] RenderSettings juliaExample();
/// Seahorse Valley at zoom 5000 in `palette`.
[[nodiscard]] RenderSettings seahorse(std::string_view palette = "electric");
/// Seahorse Valley in the fire palette with a denser, shifted cycle (density 32, offset 0.25).
[[nodiscard]] RenderSettings seahorseFire();
/// The seahorse dive's destination (a point with structure at every depth) at `zoom`.
[[nodiscard]] RenderSettings deepSeahorse(double zoom);
/// The bookmarks the screenshot mode shows in the side panel.
[[nodiscard]] std::vector<Bookmark> screenshotBookmarks();

}  // namespace mandelbrotter::app
