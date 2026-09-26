#include "gui/export_runner.h"

#include <algorithm>
#include <format>

#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/window.h>

#include "Mandelbrotter/app/ExportTask.h"
#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

bool exportPngWithProgress(wxWindow* parent, const RenderSettings& settings,
                           const ExportOptions& options, const std::filesystem::path& path)
{
    app::ExportTask task(settings, options, path);

    constexpr int    kRange = app::kExportProgressRange;
    wxProgressDialog dialog(
        "Saving image",
        toWx(std::format("Rendering {}x{}...", options.size.width, options.size.height)), kRange,
        parent, wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_AUTO_HIDE);
    bool cancelled = false;
    while (!task.waitFor(app::kExportPollInterval))
    {
        if (!cancelled && !dialog.Update(std::min(task.progress(), kRange - 1)))
        {
            cancelled = true;
            task.cancel();
            dialog.Update(kRange - 1, "Cancelling...");
        }
    }
    dialog.Update(kRange);

    switch (task.outcome())
    {
        case app::ExportTask::Outcome::SAVED:
            return true;
        case app::ExportTask::Outcome::FAILED:
            if (!cancelled)
            {
                wxMessageBox(
                    toWx(std::format("Could not save {}:\n{}", path.string(), task.error())),
                    "Save image failed", wxOK | wxICON_ERROR, parent);
            }
            return false;
        case app::ExportTask::Outcome::RUNNING:
        case app::ExportTask::Outcome::CANCELLED:
            break;
    }
    return false;
}

}  // namespace mandelbrotter::gui
