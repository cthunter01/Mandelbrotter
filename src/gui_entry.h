#pragma once

#include "Mandelbrotter/app/startup.h"

namespace mandelbrotter
{

/// Runs the window until it closes and returns the process exit code. main() has handled the
/// command line already, so the toolkit sees no arguments but the program name `argv0`. Each GUI
/// build defines it: main_wx.cpp (wxWidgets), main_headless.cpp (a build without a window).
int runGui(const app::StartupOptions& options, char* argv0);

}  // namespace mandelbrotter
