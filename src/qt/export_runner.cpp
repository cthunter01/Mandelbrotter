#include "qt/export_runner.h"

#include <QCoreApplication>
#include <QProgressDialog>
#include <QString>
#include <QWidget>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <string_view>

#include "Mandelbrotter/app/ExportTask.h"
#include "Mandelbrotter/app/ui_text.h"
#include "qt/qt_util.h"

namespace mandelbrotter::qt
{

using namespace Qt::StringLiterals;

bool exportPngWithProgress(
    QWidget* parent, const RenderSettings& settings, const ExportOptions& options,
    const std::filesystem::path&                                                 path,
    const std::function<void(std::string_view title, std::string_view message)>& reportError)
{
    app::ExportTask task(settings, options, path);

    constexpr int   kRange = app::kExportProgressRange;
    QProgressDialog dialog(toQt(app::renderingText(options.size)), u"Cancel"_s, 0, kRange, parent);
    dialog.setWindowTitle(toQt(app::kSavingTitle));
    // Application-modal: the help window waits too, as troubleshooting.html says.
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setMinimumDuration(0);  // the default waits four seconds
    dialog.setAutoReset(false);
    dialog.setAutoClose(false);
    dialog.setValue(0);
    bool canceled = false;
    while (!task.waitFor(app::kExportPollInterval))
    {
        dialog.setValue(std::min(task.progress(), kRange - 1));
        QCoreApplication::processEvents();
        if (dialog.wasCanceled() && !canceled)
        {
            canceled = true;
            task.cancel();
            dialog.setLabelText(toQt(app::kCanceling));
        }
    }
    dialog.setValue(kRange);

    switch (task.outcome())
    {
        case app::ExportTask::Outcome::SAVED:
            return true;
        case app::ExportTask::Outcome::FAILED:
            if (!canceled && reportError)
            {
                reportError(app::kSaveFailedTitle, app::saveFailedText(path, task.error()));
            }
            return false;
        case app::ExportTask::Outcome::RUNNING:
        case app::ExportTask::Outcome::CANCELED:
            break;
    }
    return false;
}

}  // namespace mandelbrotter::qt
