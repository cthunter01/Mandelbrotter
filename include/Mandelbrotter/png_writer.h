#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "Mandelbrotter/image.h"

namespace mandelbrotter
{

/// Encodes an image as an 8-bit RGB PNG. Throws std::runtime_error for an empty image or encoder
/// failure.
[[nodiscard]] std::vector<std::uint8_t> encodePng(const RgbImage& image);

/// Writes an image as a PNG file. Throws std::runtime_error if encoding or writing fails.
void writePng(const std::filesystem::path& path, const RgbImage& image);

}  // namespace mandelbrotter
