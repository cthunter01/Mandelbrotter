#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/kernel.h"
#include "Mandelbrotter/palette.h"

namespace mandelbrotter
{

/// Per-pixel iteration results, kept separately from the colours so a palette change is just a
/// recolour.
struct IterationBuffer
{
    int                width{};
    int                height{};
    std::vector<float> smoothIter;  ///< width * height, row-major
    std::vector<std::uint8_t>
        interior;  ///< 0 or 1 (not vector<bool>: tiles are written concurrently)

    IterationBuffer() = default;
    IterationBuffer(int bufferWidth, int bufferHeight);

    [[nodiscard]] PixelSize   size() const noexcept { return {width, height}; }
    [[nodiscard]] PixelRect   bounds() const noexcept { return {0, 0, width, height}; }
    [[nodiscard]] std::size_t index(int x, int y) const noexcept
    {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
               static_cast<std::size_t>(x);
    }
    [[nodiscard]] IterationResult at(int x, int y) const noexcept;
    void                          set(int x, int y, IterationResult result) noexcept;

    bool operator==(const IterationBuffer&) const = default;
};

/// 8-bit RGB image, row-major, 3 bytes per pixel, no padding (the layout wxImage and PNG writers
/// expect).
struct RgbImage
{
    int                       width{};
    int                       height{};
    std::vector<std::uint8_t> pixels;

    RgbImage() = default;
    RgbImage(int imageWidth, int imageHeight, Rgb fill = kBlack);

    [[nodiscard]] PixelSize   size() const noexcept { return {width, height}; }
    [[nodiscard]] PixelRect   bounds() const noexcept { return {0, 0, width, height}; }
    [[nodiscard]] std::size_t offset(int x, int y) const noexcept
    {
        return 3 * (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(x));
    }
    [[nodiscard]] Rgb at(int x, int y) const noexcept;
    void              set(int x, int y, Rgb color) noexcept;

    bool operator==(const RgbImage&) const = default;
};

[[nodiscard]] PixelRect intersect(PixelRect a, PixelRect b) noexcept;

/// Colour the pixels of `rect` (clipped to the buffer) into `out`, which must have the buffer's
/// size.
void colorize(const IterationBuffer& buffer, PixelRect rect, const Palette& palette,
              const ColoringSettings& settings, RgbImage& out);
void colorize(const IterationBuffer& buffer, const Palette& palette,
              const ColoringSettings& settings, RgbImage& out);
[[nodiscard]] RgbImage colorized(const IterationBuffer& buffer, const Palette& palette,
                                 const ColoringSettings& settings);

/// Box-filter downsample by an integer factor; the result is (width / factor) x (height / factor).
[[nodiscard]] RgbImage downsample(const RgbImage& image, int factor);

/// Copy `src` into `dst` displaced by (dx, dy) pixels, clipped to `dst`. Pixels not covered keep
/// their value.
void blitShifted(const RgbImage& src, int dx, int dy, RgbImage& dst) noexcept;

}  // namespace mandelbrotter
