// The Qt build's runGui(): the application object, the main window and, in the developer
// screenshot mode, the screenshot run.
#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QString>
#include <QStyle>
#include <array>
#include <filesystem>
#include <iostream>
#include <string>

#include "Mandelbrotter/app/startup.h"
#include "Mandelbrotter/app/ui_text.h"
#include "gui_entry.h"
#include "qt/MainWindow.h"
#include "qt/platform.h"
#include "qt/qt_util.h"

namespace mandelbrotter
{

using namespace Qt::StringLiterals;

int runGui(const app::StartupOptions& options, char* argv0)
{
    if (const std::string missing = qt::missingDisplay(); !missing.empty())
    {
        std::cerr << "error: " << missing << '\n';
        return 1;
    }

    // Qt gets no arguments: main() has handled them. QT_QPA_PLATFORM, QT_SCALE_FACTOR and the like
    // still work through the environment.
    int                  argc = 1;
    std::array<char*, 2> argv{argv0, nullptr};
    // No organization name: Qt would insert it into the data paths.
    QApplication::setApplicationName(qt::toQt(app::kWindowTitle));
    QApplication::setApplicationVersion(qt::toQt(MANDELBROTTER_VERSION));
    // The Wayland app_id and the X11 window class, as Mandelbrotter.desktop's StartupWMClass says.
    QGuiApplication::setDesktopFileName(qt::toQt(app::kWindowTitle));
    const QApplication application(argc, argv.data());

    // macOS shows the bundle's icon; a window icon would replace it in the Dock.
    if (QGuiApplication::platformName() != u"cocoa"_s)
    {
        QApplication::setWindowIcon(QIcon(u":/icons/mandelbrotter.png"_s));
    }
    if (options.screenshotsDir)
    {
        // The same look on every machine that takes the help book's screenshots.
        QApplication::setStyle(u"Fusion"_s);
        QApplication::setPalette(QApplication::style()->standardPalette());
    }

    const std::filesystem::path bookmarks =
        app::bookmarksPathFor(options, qt::userDataDir(), std::filesystem::temp_directory_path());
    qt::MainWindow window(options.settings, bookmarks);
    window.show();
    return QApplication::exec();
}

}  // namespace mandelbrotter
