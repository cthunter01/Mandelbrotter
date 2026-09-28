// The wxWidgets build's runGui(). wxIMPLEMENT_APP_NO_MAIN must live in the executable (not in a
// static library, where the linker would drop the app initializer object), so this file is compiled
// into it beside the toolkit-free main.cpp.
#include <array>
#include <print>
#include <string_view>

#include <wx/app.h>
#include <wx/init.h>

#include "Mandelbrotter/app/startup.h"
#include "Mandelbrotter/help_images.h"
#include "gui/MandelbrotterApp.h"
#include "gui_entry.h"

wxIMPLEMENT_APP_NO_MAIN(mandelbrotter::gui::MandelbrotterApp);

namespace mandelbrotter
{

int runGui(const app::StartupOptions& options, char* argv0)
{
    if (options.screenshotsDir)
    {
        // The rendered example pictures of the help book need no window; the window then takes
        // the screenshots (ScreenshotRun) and quits.
        std::println("Rendering help images into {}", options.screenshotsDir->string());
        writeHelpImages(*options.screenshotsDir,
                        [](std::string_view file) { std::println("  {}", file); });
    }
    gui::setStartupOptions(options);

    // wx gets no arguments: main() has handled them.
    std::array<char*, 2> argv{argv0, nullptr};
    int                  argc = 1;
    return wxEntry(argc, argv.data());
}

}  // namespace mandelbrotter
