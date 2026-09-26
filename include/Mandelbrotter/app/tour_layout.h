#pragma once

#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::app
{

/// Where the guided tour's card of size `card` goes beside `anchor`, both in the coordinates of a
/// parent whose client area is `parent`: inside the anchor's top-left corner when the anchor is a
/// large area (wider than twice the card, such as the canvas), else to its left when there is room,
/// else to its right; `gap` pixels away, top-aligned, and clamped into the parent.
[[nodiscard]] PixelPoint placeTourCard(PixelRect anchor, PixelSize card, PixelSize parent,
                                       int gap) noexcept;

}  // namespace mandelbrotter::app
