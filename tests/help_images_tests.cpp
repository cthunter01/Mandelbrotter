#include "Mandelbrotter/help_images.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/Palette.h"
#include "TempDir.h"

namespace
{

using mandelbrotter::HelpImageSpec;

bool hasPngSignature(const std::filesystem::path& path)
{
    std::ifstream       in(path, std::ios::binary);
    std::array<char, 8> header{};
    in.read(header.data(), static_cast<std::streamsize>(header.size()));
    constexpr std::array<char, 8> kSignature{'\x89', 'P', 'N', 'G', '\r', '\n', '\x1a', '\n'};
    return in.gcount() == 8 && header == kSignature;
}

TEST(HelpImages, SpecsAreUniquePngsWithKnownPrefixes)
{
    const std::vector<HelpImageSpec> specs = mandelbrotter::helpImageSpecs();
    ASSERT_FALSE(specs.empty());
    std::set<std::string> files;
    for (const HelpImageSpec& spec : specs)
    {
        SCOPED_TRACE(spec.file);
        EXPECT_TRUE(files.insert(spec.file).second) << "duplicate file";
        EXPECT_TRUE(spec.file.ends_with(".png"));
        bool knownPrefix = false;
        for (const std::string_view prefix :
             {"fractal-", "julia-", "palette-", "iterations-", "place-", "deep-"})
        {
            knownPrefix = knownPrefix || spec.file.starts_with(prefix);
        }
        EXPECT_TRUE(knownPrefix);
        EXPECT_GT(spec.size.width, 0);
        EXPECT_GT(spec.size.height, 0);
        EXPECT_TRUE(spec.supersample == 1 || spec.supersample == 2 || spec.supersample == 4);
        EXPECT_NE(mandelbrotter::findPalette(spec.settings.coloring.palette), nullptr);
    }
    for (const std::string_view palette : mandelbrotter::paletteNames())
    {
        EXPECT_TRUE(files.contains("palette-" + std::string(palette) + ".png")) << palette;
    }
}

TEST(HelpImages, WritesEveryImage)
{
    const mandelbrotter::test::TempDir dir;
    std::vector<std::string>           reported;
    mandelbrotter::writeHelpImages(
        dir.path(), [&](std::string_view file) { reported.emplace_back(file); }, 0.05);
    const std::vector<HelpImageSpec> specs = mandelbrotter::helpImageSpecs();
    ASSERT_EQ(reported.size(), specs.size());
    for (std::size_t i = 0; i < specs.size(); ++i)
    {
        SCOPED_TRACE(specs[i].file);
        EXPECT_EQ(reported[i], specs[i].file);
        const std::filesystem::path path = dir.path() / specs[i].file;
        ASSERT_TRUE(std::filesystem::exists(path));
        EXPECT_TRUE(hasPngSignature(path));
    }
}

}  // namespace
