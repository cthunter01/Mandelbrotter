#include "qt/platform.h"

#include <QDir>
#include <QStandardPaths>
#include <QtGlobal>
#include <filesystem>
#include <string>

#include "qt/qt_util.h"

#ifdef __linux__
#elifdef __APPLE__
#elifdef _WIN32
#else
#error "platform.cpp needs a branch for this platform"
#endif

namespace mandelbrotter::qt
{

#ifdef __linux__

std::filesystem::path userDataDir()
{
    return toPath(QDir::homePath()) / ".Mandelbrotter";
}

std::string missingDisplay()
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") && qEnvironmentVariableIsEmpty("DISPLAY") &&
        qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
    {
        return "no display to open a window on (use --render to write a picture without one)";
    }
    return {};
}

#elifdef __APPLE__

std::filesystem::path userDataDir()
{
    return toPath(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)) /
           "Mandelbrotter";
}

std::string missingDisplay()
{
    return {};
}

#elifdef _WIN32

std::filesystem::path userDataDir()
{
    // %APPDATA%\<application name>: no organization name is set, so Qt inserts none.
    return toPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
}

std::string missingDisplay()
{
    return {};
}

#endif

}  // namespace mandelbrotter::qt
