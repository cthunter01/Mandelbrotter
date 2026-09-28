// The executable's entry point, free of any toolkit. The command line is handled here, so the CLI
// path never initializes a GUI and works without a display; the window is runGui()'s (gui_entry.h),
// which the GUI build defines.
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "Mandelbrotter/app/startup.h"
#include "gui_entry.h"

namespace
{

int run(int argc, char** argv)
{
    const std::span                     rawArgs(argv, static_cast<std::size_t>(argc));
    const std::vector<std::string_view> args(rawArgs.begin() + 1, rawArgs.end());
    const auto prepared = mandelbrotter::app::prepareStartup(args, std::cout, std::cerr);
    if (const int* exitCode = std::get_if<int>(&prepared))
    {
        return *exitCode;
    }
    return mandelbrotter::runGui(std::get<mandelbrotter::app::StartupOptions>(prepared),
                                 rawArgs.front());
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
