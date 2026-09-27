#include "Mandelbrotter/app/ExportTask.h"

#define STBI_NO_STDIO
#include <stb_image.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/exporter.h"
#include "Mandelbrotter/image.h"
#include "TempDir.h"

namespace
{

using mandelbrotter::ExportOptions;
using mandelbrotter::RenderSettings;
using mandelbrotter::RgbImage;
using mandelbrotter::app::ExportTask;
using mandelbrotter::test::TempDir;
using Outcome = ExportTask::Outcome;

RenderSettings scene()
{
    RenderSettings settings;
    settings.view           = {{-0.75, 0.1}, 3.0};
    settings.maxIterations  = 100;
    settings.autoIterations = false;
    return settings;
}

/// Big enough that nothing finishes before the test has canceled it.
constexpr ExportOptions kSlow{.size = {4000, 3000}, .supersample = 4};

/// Polls like the toolkit does, up to a deadline.
bool waitUntilFinished(ExportTask& task)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (task.waitFor(mandelbrotter::app::kExportPollInterval))
        {
            return true;
        }
    }
    return false;
}

struct StbFree
{
    void operator()(stbi_uc* p) const { stbi_image_free(p); }
};

std::optional<RgbImage> readPng(const std::filesystem::path& path)
{
    std::ifstream                           file(path, std::ios::binary);
    const std::vector<std::uint8_t>         bytes((std::istreambuf_iterator<char>(file)),
                                                  std::istreambuf_iterator<char>());
    int                                     width    = 0;
    int                                     height   = 0;
    int                                     channels = 0;
    const std::unique_ptr<stbi_uc, StbFree> pixels(stbi_load_from_memory(
        bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, 3));
    if (!pixels)
    {
        return std::nullopt;
    }
    RgbImage image(width, height);
    std::copy_n(pixels.get(), image.pixels.size(), image.pixels.begin());
    return image;
}

TEST(ExportTask, WritesThePngAndReportsFullProgress)
{
    const TempDir       dir;
    const ExportOptions options{.size = {90, 60}, .supersample = 2};
    ExportTask          task(scene(), options, dir / "out.png");
    ASSERT_TRUE(waitUntilFinished(task));
    EXPECT_EQ(task.outcome(), Outcome::SAVED);
    EXPECT_EQ(task.error(), "");
    EXPECT_EQ(task.progress(), mandelbrotter::app::kExportProgressRange);

    const std::optional<RgbImage> written = readPng(dir / "out.png");
    ASSERT_TRUE(written.has_value());
    const std::optional<RgbImage> expected = mandelbrotter::renderForExport(scene(), options);
    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(*written, *expected);

    task.cancel();  // too late: the file is written
    EXPECT_EQ(task.outcome(), Outcome::SAVED);
}

TEST(ExportTask, CancelingStopsBeforeTheFileIsWritten)
{
    const TempDir dir;
    ExportTask    task(scene(), kSlow, dir / "out.png");
    task.cancel();
    ASSERT_TRUE(waitUntilFinished(task));
    EXPECT_EQ(task.outcome(), Outcome::CANCELED);
    EXPECT_EQ(task.error(), "");
    EXPECT_LT(task.progress(), mandelbrotter::app::kExportProgressRange);
    EXPECT_FALSE(std::filesystem::exists(dir / "out.png"));
}

TEST(ExportTask, AnUnwritablePathFails)
{
    const TempDir dir;
    std::ofstream(dir / "file") << "a file, not a directory";
    ExportTask task(scene(), {.size = {20, 10}, .supersample = 1}, dir / "file" / "out.png");
    ASSERT_TRUE(waitUntilFinished(task));
    EXPECT_EQ(task.outcome(), Outcome::FAILED);
    EXPECT_NE(task.error(), "");
}

TEST(ExportTask, AnEmptySizeFails)
{
    const TempDir dir;
    ExportTask    task(scene(), {.size = {0, 10}, .supersample = 1}, dir / "out.png");
    ASSERT_TRUE(waitUntilFinished(task));
    EXPECT_EQ(task.outcome(), Outcome::FAILED);
    EXPECT_NE(task.error(), "");
    EXPECT_FALSE(std::filesystem::exists(dir / "out.png"));
}

TEST(ExportTask, IsRunningUntilItFinishes)
{
    const TempDir dir;
    ExportTask    task(scene(), kSlow, dir / "out.png");
    EXPECT_FALSE(task.waitFor(std::chrono::milliseconds(0)));
    EXPECT_EQ(task.outcome(), Outcome::RUNNING);
    task.cancel();
    ASSERT_TRUE(waitUntilFinished(task));
    EXPECT_TRUE(task.waitFor(std::chrono::milliseconds(0)));  // stays finished
}

TEST(ExportTask, DestroyingARunningTaskStopsAndJoinsIt)
{
    const TempDir dir;
    const auto    start = std::chrono::steady_clock::now();
    {
        const ExportTask task(scene(), kSlow, dir / "out.png");
    }
    EXPECT_FALSE(std::filesystem::exists(dir / "out.png"));
    // The full render would take far longer; the destructor canceled it.
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(30));
}

}  // namespace
