#include "gui/export_runner.h"

#include <algorithm>

#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/window.h>

#include "Mandelbrotter/app/ExportTask.h"
#include "Mandelbrotter/app/ui_text.h"
#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

bool exportPngWithProgress(wxWindow* parent, const RenderSettings& settings,
                           const ExportOptions& options, const std::filesystem::path& path)
{
    app::ExportTask task(settings, options, path);

    constexpr int    kRange = app::kExportProgressRange;
    wxProgressDialog dialog(toWx(app::kSavingTitle), toWx(app::renderingText(options.size)), kRange,
                            parent, wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_AUTO_HIDE);
    bool             canceled = false;
    while (!task.waitFor(app::kExportPollInterval))
    {
        if (!canceled && !dialog.Update(std::min(task.progress(), kRange - 1)))
        {
            canceled = true;
            task.cancel();
            dialog.Update(kRange - 1, toWx(app::kCanceling));
        }
    }
    dialog.Update(kRange);

    switch (task.outcome())
    {
        case app::ExportTask::Outcome::SAVED:
            return true;
        case app::ExportTask::Outcome::FAILED:
            if (!canceled)
            {
                wxMessageBox(toWx(app::saveFailedText(path, task.error())),
                             toWx(app::kSaveFailedTitle), wxOK | wxICON_ERROR, parent);
            }
            return false;
        case app::ExportTask::Outcome::RUNNING:
        case app::ExportTask::Outcome::CANCELED:
            break;
    }
    return false;
}

}  // namespace mandelbrotter::gui
