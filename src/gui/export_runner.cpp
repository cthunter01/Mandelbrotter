#include "gui/export_runner.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <format>
#include <future>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/utils.h>
#include <wx/window.h>

#include "Mandelbrotter/png_writer.h"
#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

namespace
{

constexpr int  kProgressRange = 1000;
constexpr auto kPollInterval  = std::chrono::milliseconds(50);

}  // namespace

bool exportPngWithProgress(wxWindow* parent, const RenderSettings& settings,
                           const ExportOptions& options, const std::filesystem::path& path)
{
    std::atomic<int>                         progress{0};
    std::promise<std::optional<std::string>> outcome;  // nullopt = success, string = error message
    std::future<std::optional<std::string>>  result = outcome.get_future();

    std::jthread worker([&](const std::stop_token& stop) {
        try
        {
            const std::optional<RgbImage> image =
                renderForExport(settings, options, stop, [&](int done, int total) {
                    progress.store(total > 0 ? (done * kProgressRange) / total : 0);
                });
            if (!image)
            {
                outcome.set_value("cancelled");
                return;
            }
            writePng(path, *image);
            outcome.set_value(std::nullopt);
        }
        catch (const std::exception& e)
        {
            outcome.set_value(e.what());
        }
    });

    wxProgressDialog dialog(
        "Saving image",
        toWx(std::format("Rendering {}x{}...", options.size.width, options.size.height)),
        kProgressRange, parent, wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_AUTO_HIDE);
    bool cancelled = false;
    while (result.wait_for(kPollInterval) != std::future_status::ready)
    {
        if (!cancelled && !dialog.Update(std::min(progress.load(), kProgressRange - 1)))
        {
            cancelled = true;
            worker.request_stop();
            dialog.Update(kProgressRange - 1, "Cancelling...");
        }
    }
    worker.join();
    dialog.Update(kProgressRange);

    const std::optional<std::string> error = result.get();
    if (!error)
    {
        return true;
    }
    if (!cancelled)
    {
        wxMessageBox(toWx(std::format("Could not save {}:\n{}", path.string(), *error)),
                     "Save image failed", wxOK | wxICON_ERROR, parent);
    }
    return false;
}

}  // namespace mandelbrotter::gui
