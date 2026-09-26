#include "Mandelbrotter/app/tour_layout.h"

#include <algorithm>

#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::app
{

PixelPoint placeTourCard(PixelRect anchor, PixelSize card, PixelSize parent, int gap) noexcept
{
    PixelPoint at;
    if (anchor.width > 2 * card.width)
    {
        at = {anchor.x + gap, anchor.y + gap};
    }
    else if (anchor.x - card.width - gap >= 0)
    {
        at = {anchor.x - card.width - gap, anchor.y};
    }
    else
    {
        // From the anchor's last column, as wxRect::GetRight() counts.
        at = {anchor.right() - 1 + gap, anchor.y};
    }
    at.x = std::clamp(at.x, 0, std::max(0, parent.width - card.width));
    at.y = std::clamp(at.y, 0, std::max(0, parent.height - card.height));
    return at;
}

}  // namespace mandelbrotter::app
