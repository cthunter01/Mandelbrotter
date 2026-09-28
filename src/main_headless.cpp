// The runGui() of a build without a window (MANDELBROTTER_GUI=none): the command line works, the
// window does not exist.
#include <iostream>

#include "Mandelbrotter/app/startup.h"
#include "gui_entry.h"

namespace mandelbrotter
{

int runGui(const app::StartupOptions& /*options*/, char* /*argv0*/)
{
    std::cerr << "error: this build has no window; use --render to write a picture (see --help)\n";
    return 2;
}

}  // namespace mandelbrotter
