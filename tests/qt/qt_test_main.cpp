// The Qt view tests' main(): a QApplication on the offscreen platform, then GoogleTest.
#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QString>
#include <QtGlobal>
#include <filesystem>

#include <gtest/gtest.h>

#include "Mandelbrotter/app/ui_text.h"
#include "qt/qt_util.h"

namespace qt = mandelbrotter::qt;

int main(int argc, char** argv)
{
    // Before anything else: ctest lists the tests (--gtest_list_tests) with its own environment,
    // and QApplication aborts where there is no display, as on a CI container.
    const bool                  offscreen = qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM");
    const std::filesystem::path config{MANDELBROTTER_QT_OFFSCREEN_CONFIG};
    const QString               workingDir = QDir::currentPath();
    if (offscreen)
    {
        // The platform's arguments are separated by colons, so the config file is named relative
        // to its directory: an absolute Windows path would be cut at its drive letter. The plugin
        // reads the file while QApplication is built.
        qputenv("QT_QPA_PLATFORM",
                QByteArray("offscreen:configfile=") + config.filename().string().c_str());
        QDir::setCurrent(qt::fromPath(config.parent_path()));
    }
    QApplication::setApplicationName(qt::toQt(mandelbrotter::app::kWindowTitle));
    const QApplication application(argc, argv);
    if (offscreen)
    {
        QDir::setCurrent(workingDir);
    }
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
