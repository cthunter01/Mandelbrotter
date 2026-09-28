#include "qt/ScreenshotRun.h"

#include <QColor>
#include <QImage>
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "Mandelbrotter/app/ScreenshotScript.h"
#include "Mandelbrotter/app/scenes.h"
#include "Mandelbrotter/app/startup.h"
#include "QtHarness.h"
#include "TempDir.h"
#include "qt/MainWindow.h"
#include "qt/qt_util.h"

namespace
{

using mandelbrotter::test::pumpUntil;
using mandelbrotter::test::TempDir;
namespace app = mandelbrotter::app;
namespace qt  = mandelbrotter::qt;

/// True when every pixel has the same color.
bool uniform(const QImage& image)
{
    const QColor first = image.pixelColor(0, 0);
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            if (image.pixelColor(x, y) != first)
            {
                return false;
            }
        }
    }
    return true;
}

TEST(ScreenshotRun, TakesEveryPicture)
{
    const TempDir  temp;
    const auto     bookmarks = app::scratchBookmarksPath(temp.path());
    qt::MainWindow window(app::mandelbrotDefault(), bookmarks);
    window.show();
    qt::ScreenshotRun run(window, temp / "shots");
    run.start();
    ASSERT_TRUE(pumpUntil([&run] { return run.done(); }, std::chrono::minutes(10)));
    EXPECT_EQ(run.exitCode(), 0);
    EXPECT_FALSE(std::filesystem::exists(bookmarks.parent_path()));  // the scratch bookmarks
    for (const std::string_view file : app::ScreenshotScript::files())
    {
        SCOPED_TRACE(file);
        const QImage image(qt::fromPath(temp / "shots" / std::string(file)));
        ASSERT_FALSE(image.isNull());
        EXPECT_LE(image.width(), app::kScreenshotMaxWidth);
        EXPECT_GT(image.height(), 10);
        EXPECT_FALSE(uniform(image));
    }
    ASSERT_TRUE(pumpUntil([&window] { return !window.isVisible(); }));
}

}  // namespace
