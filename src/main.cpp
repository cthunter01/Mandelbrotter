// The executable's only translation unit. wxIMPLEMENT_APP_NO_MAIN must live here (not in a static
// library, where the linker would drop the app initializer object). main() decides between the
// headless CLI and the GUI, so the CLI path never initialises GTK and works without a display.
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <print>
#include <span>
#include <string_view>
#include <vector>

#include <wx/app.h>
#include <wx/init.h>

#include "Mandelbrotter/cli.h"
#include "Mandelbrotter/help_images.h"
#include "gui/MandelbrotterApp.h"

wxIMPLEMENT_APP_NO_MAIN(mandelbrotter::gui::MandelbrotterApp);

namespace
{

int run(int argc, char** argv)
{
    const std::span               rawArgs(argv, static_cast<std::size_t>(argc));
    std::vector<std::string_view> args(rawArgs.begin() + 1, rawArgs.end());
    // macOS has passed "-psn_<process serial number>" to an app bundle started from Finder; it is
    // not ours.
    std::erase_if(args, [](std::string_view arg) { return arg.starts_with("-psn_"); });

    const auto options = mandelbrotter::parseCommandLine(args);
    if (!options)
    {
        std::println(stderr, "error: {}\n\n{}", options.error(), mandelbrotter::usageText());
        return 2;
    }
    if (!options->wantsGui())
    {
        return mandelbrotter::runCli(*options, std::cout, std::cerr);
    }

    const auto settings = mandelbrotter::resolveSettings(*options);
    if (!settings)
    {
        std::println(stderr, "error: {}", settings.error());
        return 2;
    }
    if (options->screenshotsDir)
    {
        // The rendered example pictures of the help book need no window; the window then takes
        // the screenshots (ScreenshotRun) and quits.
        std::println("Rendering help images into {}", options->screenshotsDir->string());
        mandelbrotter::writeHelpImages(*options->screenshotsDir,
                                       [](std::string_view file) { std::println("  {}", file); });
    }
    mandelbrotter::gui::setStartupOptions(
        {.settings = *settings, .screenshotsDir = options->screenshotsDir});

    // wx gets no arguments: everything was handled above.
    int wxArgc = 1;
    return wxEntry(wxArgc, argv);
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        return run(argc, argv);
    }
    catch (const std::exception& e)
    {
        std::fputs("error: ", stderr);
        std::fputs(e.what(), stderr);
        std::fputs("\n", stderr);
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::fputs("error: unknown exception\n", stderr);
        return EXIT_FAILURE;
    }
}
