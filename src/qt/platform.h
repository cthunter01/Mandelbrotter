#pragma once

#include <filesystem>
#include <string>

/// What the Qt layer needs to know about the platforms; the layer's only #ifdef split
/// (platform.cpp).
namespace mandelbrotter::qt
{

/// The directory the bookmarks live in: the one wx 3.2's wxStandardPaths::GetUserDataDir() names,
/// so the wx and Qt builds share one file. Linux and FreeBSD: ~/.Mandelbrotter (wx ignores XDG
/// here); macOS: ~/Library/Application Support/Mandelbrotter; Windows: %APPDATA%\Mandelbrotter.
/// Needs the application name ("Mandelbrotter") set first.
[[nodiscard]] std::filesystem::path userDataDir();

/// Why no window can be opened, or empty when one can: on Linux and FreeBSD without an X11 or
/// Wayland display (and no QT_QPA_PLATFORM) Qt would abort instead of reporting it. Always empty
/// on macOS and Windows.
[[nodiscard]] std::string missingDisplay();

}  // namespace mandelbrotter::qt
