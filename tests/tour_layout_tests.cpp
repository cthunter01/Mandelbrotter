#include "Mandelbrotter/app/tour_layout.h"

#include <gtest/gtest.h>

#include "Mandelbrotter/geometry.h"

namespace
{

using mandelbrotter::PixelPoint;
using mandelbrotter::PixelSize;
using mandelbrotter::app::placeTourCard;

constexpr PixelSize kCard{300, 200};
constexpr PixelSize kParent{1280, 800};
constexpr int       kGap = 12;

TEST(TourLayout, AWideAnchorTakesTheCardInsideItsTopLeftCorner)
{
    // The canvas: more than twice the card's width.
    EXPECT_EQ(placeTourCard({0, 0, 950, 800}, kCard, kParent, kGap), (PixelPoint{12, 12}));
    EXPECT_EQ(placeTourCard({40, 30, 601, 100}, kCard, kParent, kGap), (PixelPoint{52, 42}));
}

TEST(TourLayout, TheCardGoesLeftOfANarrowAnchorWhenThereIsRoom)
{
    // A panel section on the right: top-aligned, the gap between card and anchor.
    EXPECT_EQ(placeTourCard({950, 120, 320, 180}, kCard, kParent, kGap), (PixelPoint{638, 120}));
    // Exactly enough room.
    EXPECT_EQ(placeTourCard({312, 50, 200, 80}, kCard, kParent, kGap), (PixelPoint{0, 50}));
}

TEST(TourLayout, OtherwiseTheCardGoesRight)
{
    // One pixel short of room on the left; the card starts `gap` beyond the anchor's last column.
    EXPECT_EQ(placeTourCard({311, 50, 200, 80}, kCard, kParent, kGap), (PixelPoint{522, 50}));
    EXPECT_EQ(placeTourCard({10, 50, 200, 80}, kCard, kParent, kGap), (PixelPoint{221, 50}));
}

TEST(TourLayout, TheCardIsClampedIntoTheParent)
{
    // Below the bottom edge: moved up.
    EXPECT_EQ(placeTourCard({950, 700, 320, 90}, kCard, kParent, kGap), (PixelPoint{638, 600}));
    // Right of the right edge: moved left.
    EXPECT_EQ(placeTourCard({200, 50, 250, 80}, kCard, {600, 800}, kGap), (PixelPoint{300, 50}));
    // A parent smaller than the card: the top-left corner.
    EXPECT_EQ(placeTourCard({950, 700, 320, 90}, kCard, {100, 100}, kGap), (PixelPoint{0, 0}));
}

}  // namespace
