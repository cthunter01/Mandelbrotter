#include "Mandelbrotter/image.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/kernel.h"
#include "Mandelbrotter/palette.h"

namespace mandelbrotter
{

IterationBuffer::IterationBuffer(int bufferWidth, int bufferHeight)
  : width(std::max(bufferWidth, 0)),
    height(std::max(bufferHeight, 0)),
    smoothIter(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0.0F),
    interior(smoothIter.size(), 0)
{
}

IterationResult IterationBuffer::at(int x, int y) const noexcept
{
    const std::size_t i = index(x, y);
    return {.smoothIter = static_cast<double>(smoothIter[i]), .interior = interior[i] != 0};
}

void IterationBuffer::set(int x, int y, IterationResult result) noexcept
{
    const std::size_t i = index(x, y);
    smoothIter[i]       = static_cast<float>(result.smoothIter);
    interior[i]         = result.interior ? 1 : 0;
}

RgbImage::RgbImage(int imageWidth, int imageHeight, Rgb fill)
  : width(std::max(imageWidth, 0)), height(std::max(imageHeight, 0))
{
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    pixels.resize(3 * count);
    for (std::size_t i = 0; i < count; ++i)
    {
        pixels[3 * i]       = fill.r;
        pixels[(3 * i) + 1] = fill.g;
        pixels[(3 * i) + 2] = fill.b;
    }
}

Rgb RgbImage::at(int x, int y) const noexcept
{
    const std::size_t i = offset(x, y);
    return {pixels[i], pixels[i + 1], pixels[i + 2]};
}

void RgbImage::set(int x, int y, Rgb color) noexcept
{
    const std::size_t i = offset(x, y);
    pixels[i]           = color.r;
    pixels[i + 1]       = color.g;
    pixels[i + 2]       = color.b;
}

PixelRect intersect(PixelRect a, PixelRect b) noexcept
{
    const int left   = std::max(a.x, b.x);
    const int top    = std::max(a.y, b.y);
    const int right  = std::min(a.right(), b.right());
    const int bottom = std::min(a.bottom(), b.bottom());
    if (right <= left || bottom <= top)
    {
        return {};
    }
    return {left, top, right - left, bottom - top};
}

void colorize(const IterationBuffer& buffer, PixelRect rect, const Palette& palette,
              const ColoringSettings& settings, RgbImage& out)
{
    if (out.size() != buffer.size())
    {
        out = RgbImage(buffer.width, buffer.height);
    }
    const PixelRect clipped = intersect(rect, buffer.bounds());
    for (int y = clipped.y; y < clipped.bottom(); ++y)
    {
        for (int x = clipped.x; x < clipped.right(); ++x)
        {
            out.set(x, y, colorFor(buffer.at(x, y), palette, settings));
        }
    }
}

void colorize(const IterationBuffer& buffer, const Palette& palette,
              const ColoringSettings& settings, RgbImage& out)
{
    colorize(buffer, buffer.bounds(), palette, settings, out);
}

RgbImage colorized(const IterationBuffer& buffer, const Palette& palette,
                   const ColoringSettings& settings)
{
    RgbImage out(buffer.width, buffer.height);
    colorize(buffer, palette, settings, out);
    return out;
}

RgbImage downsample(const RgbImage& image, int factor)
{
    if (factor <= 1)
    {
        return image;
    }
    RgbImage   out(image.width / factor, image.height / factor);
    const auto samples = static_cast<unsigned>(factor * factor);
    for (int y = 0; y < out.height; ++y)
    {
        for (int x = 0; x < out.width; ++x)
        {
            unsigned sumR = 0;
            unsigned sumG = 0;
            unsigned sumB = 0;
            for (int sy = 0; sy < factor; ++sy)
            {
                for (int sx = 0; sx < factor; ++sx)
                {
                    const Rgb c = image.at((x * factor) + sx, (y * factor) + sy);
                    sumR += c.r;
                    sumG += c.g;
                    sumB += c.b;
                }
            }
            const unsigned half = samples / 2;
            out.set(x, y,
                    {static_cast<std::uint8_t>((sumR + half) / samples),
                     static_cast<std::uint8_t>((sumG + half) / samples),
                     static_cast<std::uint8_t>((sumB + half) / samples)});
        }
    }
    return out;
}

void blitShifted(const RgbImage& src, int dx, int dy, RgbImage& dst) noexcept
{
    // Destination rect covered by the shifted source, clipped to dst.
    const PixelRect target = intersect({dx, dy, src.width, src.height}, dst.bounds());
    if (target.empty())
    {
        return;
    }
    const auto rowBytes = static_cast<std::size_t>(target.width) * 3;
    for (int y = target.y; y < target.bottom(); ++y)
    {
        const std::size_t from = src.offset(target.x - dx, y - dy);
        const std::size_t to   = dst.offset(target.x, y);
        std::memcpy(&dst.pixels[to], &src.pixels[from], rowBytes);
    }
}

}  // namespace mandelbrotter
