// The Qt view tests' main(): a QApplication on the offscreen platform, then GoogleTest.
#include <QApplication>
#include <QByteArray>
#include <QtGlobal>

#include <gtest/gtest.h>

#include "Mandelbrotter/app/ui_text.h"
#include "qt/qt_util.h"

int main(int argc, char** argv)
{
    // Before anything else: ctest lists the tests (--gtest_list_tests) with its own environment,
    // and QApplication aborts where there is no display, as on a CI container.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    {
        qputenv("QT_QPA_PLATFORM",
                QByteArray("offscreen:configfile=") + MANDELBROTTER_QT_OFFSCREEN_CONFIG);
    }
    QApplication::setApplicationName(mandelbrotter::qt::toQt(mandelbrotter::app::kWindowTitle));
    const QApplication application(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
