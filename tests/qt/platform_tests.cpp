#include "qt/platform.h"

#include <QDir>
#include <QStandardPaths>
#include <QtGlobal>
#include <filesystem>

#include <gtest/gtest.h>

#include "qt/qt_util.h"

namespace
{

namespace qt = mandelbrotter::qt;

TEST(Platform, BookmarksLiveWhereTheWxBuildKeepsThem)
{
    const std::filesystem::path dir = qt::userDataDir();
#if defined(__linux__) || defined(__FreeBSD__)
    EXPECT_EQ(dir, qt::toPath(QDir::homePath()) / ".Mandelbrotter");
#elifdef __APPLE__
    EXPECT_EQ(dir,
              qt::toPath(QDir::homePath()) / "Library" / "Application Support" / "Mandelbrotter");
#elifdef _WIN32
    EXPECT_EQ(dir, qt::toPath(qEnvironmentVariable("APPDATA")) / "Mandelbrotter");
#endif
}

TEST(Platform, TheTestsHaveADisplay)
{
    EXPECT_TRUE(qt::missingDisplay().empty());  // QT_QPA_PLATFORM is set
}

}  // namespace
