#include "Mandelbrotter/png_writer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <span>
#include <stdexcept>
#include <vector>

#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/image.h"

#define STBI_WRITE_NO_STDIO  // declarations only here; the implementation is compiled in stb::stb
#include <stb_image_write.h>

namespace mandelbrotter
{

namespace
{

void appendBytes(void* context, void* data, int size)
{
    auto*                               out = static_cast<std::vector<std::uint8_t>*>(context);
    const std::span<const std::uint8_t> bytes(static_cast<const std::uint8_t*>(data),
                                              static_cast<std::size_t>(std::max(size, 0)));
    out->insert(out->end(), bytes.begin(), bytes.end());
}

}  // namespace

std::vector<std::uint8_t> encodePng(const RgbImage& image)
{
    if (image.size().empty())
    {
        throw std::runtime_error("cannot encode an empty image");
    }
    std::vector<std::uint8_t> bytes;
    const int ok = stbi_write_png_to_func(appendBytes, &bytes, image.width, image.height, 3,
                                          image.pixels.data(), image.width * 3);
    if (ok == 0)
    {
        throw std::runtime_error("PNG encoding failed");
    }
    return bytes;
}

void writePng(const std::filesystem::path& path, const RgbImage& image)
{
    const std::vector<std::uint8_t> bytes = encodePng(image);
    std::ofstream                   file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        throw std::runtime_error("cannot open " + path.string() + " for writing");
    }
    std::ranges::transform(bytes, std::ostreambuf_iterator<char>(file),
                           [](std::uint8_t byte) { return static_cast<char>(byte); });
    file.flush();
    if (!file)
    {
        throw std::runtime_error("failed to write " + path.string());
    }
}

}  // namespace mandelbrotter
