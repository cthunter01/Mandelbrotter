#include "Mandelbrotter/png_writer.h"

#define STBI_NO_STDIO
#include <stb_image.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/image.h"
#include "test_util.h"

namespace
{

using mandelbrotter::RgbImage;

RgbImage testImage()
{
    RgbImage image(5, 3);
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 5; ++x)
        {
            image.set(x, y,
                      {static_cast<std::uint8_t>(x * 50), static_cast<std::uint8_t>(y * 100),
                       static_cast<std::uint8_t>(255 - (x * 10))});
        }
    }
    return image;
}

struct StbFree
{
    void operator()(stbi_uc* p) const { stbi_image_free(p); }
};

RgbImage decode(const std::vector<std::uint8_t>& bytes)
{
    int                                     width    = 0;
    int                                     height   = 0;
    int                                     channels = 0;
    const std::unique_ptr<stbi_uc, StbFree> pixels(stbi_load_from_memory(
        bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, 3));
    if (!pixels)
    {
        throw std::runtime_error("decode failed");
    }
    RgbImage image(width, height);
    std::copy_n(pixels.get(), image.pixels.size(), image.pixels.begin());
    return image;
}

TEST(PngWriter, EncodedBytesStartWithThePngSignatureAndIhdr)
{
    const std::vector<std::uint8_t> bytes = mandelbrotter::encodePng(testImage());
    const std::vector<std::uint8_t> signature{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    ASSERT_GT(bytes.size(), 33U);
    EXPECT_TRUE(std::equal(signature.begin(), signature.end(), bytes.begin()));
    EXPECT_EQ(std::string(bytes.begin() + 12, bytes.begin() + 16), "IHDR");
    // Width and height are big-endian 32-bit ints right after the IHDR tag.
    EXPECT_EQ(bytes[19], 5);
    EXPECT_EQ(bytes[23], 3);
    EXPECT_EQ(bytes[24], 8);  // bit depth
    EXPECT_EQ(bytes[25], 2);  // colour type: RGB
}

TEST(PngWriter, RoundTripsPixelsExactly)
{
    const RgbImage image = testImage();
    EXPECT_EQ(decode(mandelbrotter::encodePng(image)), image);
}

TEST(PngWriter, WritesAFileThatDecodes)
{
    const mandelbrotter::test::TempDir dir;
    const auto                         path = dir / "out.png";
    mandelbrotter::writePng(path, testImage());
    std::ifstream file(path, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                          std::istreambuf_iterator<char>());
    EXPECT_EQ(decode(bytes), testImage());
}

TEST(PngWriter, RejectsEmptyImagesAndUnwritablePaths)
{
    EXPECT_THROW(static_cast<void>(mandelbrotter::encodePng(RgbImage{})), std::runtime_error);
    const mandelbrotter::test::TempDir dir;
    EXPECT_THROW(mandelbrotter::writePng(dir / "no-such-dir" / "out.png", testImage()),
                 std::runtime_error);
}

}  // namespace
