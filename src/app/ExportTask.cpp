#include "Mandelbrotter/app/ExportTask.h"

#include <chrono>
#include <exception>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/exporter.h"
#include "Mandelbrotter/image.h"
#include "Mandelbrotter/png_writer.h"

namespace mandelbrotter::app
{

ExportTask::ExportTask(RenderSettings settings, ExportOptions options, std::filesystem::path path)
  : m_settings(std::move(settings)),
    m_options(options),
    m_path(std::move(path)),
    m_worker([this](const std::stop_token& stop) { run(stop); })
{
}

ExportTask::~ExportTask()
{
    m_worker.request_stop();
    if (m_worker.joinable())
    {
        m_worker.join();
    }
}

void ExportTask::run(const std::stop_token& stop)
{
    try
    {
        const std::optional<RgbImage> image =
            renderForExport(m_settings, m_options, stop, [this](int done, int total) {
                m_progress.store(total > 0 ? (done * kExportProgressRange) / total : 0);
            });
        if (!image)
        {
            finish(Outcome::CANCELLED, {});
            return;
        }
        writePng(m_path, *image);
        finish(Outcome::SAVED, {});
    }
    catch (const std::exception& e)
    {
        finish(Outcome::FAILED, e.what());
    }
}

void ExportTask::finish(Outcome outcome, std::string error)
{
    {
        const std::scoped_lock lock(m_mutex);
        m_outcome = outcome;
        m_error   = std::move(error);
    }
    m_finished.notify_all();
}

int ExportTask::progress() const noexcept
{
    return m_progress.load();
}

bool ExportTask::waitFor(std::chrono::milliseconds timeout)
{
    std::unique_lock lock(m_mutex);
    return m_finished.wait_for(lock, timeout, [this] { return m_outcome != Outcome::RUNNING; });
}

void ExportTask::cancel()
{
    m_worker.request_stop();
}

ExportTask::Outcome ExportTask::outcome() const
{
    const std::scoped_lock lock(m_mutex);
    return m_outcome;
}

const std::string& ExportTask::error() const
{
    const std::scoped_lock lock(m_mutex);
    return m_error;
}

}  // namespace mandelbrotter::app
