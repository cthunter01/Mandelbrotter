#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/exporter.h"

namespace mandelbrotter::app
{

/// The progress a toolkit's progress dialog shows goes up to this.
inline constexpr int kExportProgressRange = 1000;
/// How often the toolkit polls a running export (and keeps its dialog responsive).
inline constexpr auto kExportPollInterval = std::chrono::milliseconds(50);

/// Save image as PNG: renders the view at the export size and writes the PNG on a worker thread,
/// which starts with the task. The toolkit polls it from its own thread (waitFor, progress) and
/// may cancel it; destroying a running task cancels it and waits for the worker.
class ExportTask
{
public:
    enum class Outcome : std::uint8_t
    {
        RUNNING,
        SAVED,
        CANCELLED,  ///< stopped before the file was written
        FAILED,     ///< see error()
    };

    ExportTask(RenderSettings settings, ExportOptions options, std::filesystem::path path);
    ~ExportTask();
    ExportTask(const ExportTask&)            = delete;
    ExportTask& operator=(const ExportTask&) = delete;
    ExportTask(ExportTask&&)                 = delete;
    ExportTask& operator=(ExportTask&&)      = delete;

    /// How far the render is, from 0 to kExportProgressRange.
    [[nodiscard]] int progress() const noexcept;
    /// Waits up to `timeout` for the task to finish; true once it has.
    [[nodiscard]] bool waitFor(std::chrono::milliseconds timeout);
    /// Asks the worker to stop. The outcome is CANCELLED unless the file was already written.
    void cancel();

    [[nodiscard]] Outcome outcome() const;
    /// The reason for FAILED (empty otherwise); stable once the task has finished.
    [[nodiscard]] const std::string& error() const;

private:
    void run(const std::stop_token& stop);
    void finish(Outcome outcome, std::string error);

    RenderSettings          m_settings;
    ExportOptions           m_options;
    std::filesystem::path   m_path;
    std::atomic<int>        m_progress{0};
    mutable std::mutex      m_mutex;
    std::condition_variable m_finished;
    Outcome                 m_outcome{Outcome::RUNNING};
    std::string             m_error;
    std::jthread            m_worker;  ///< last: starts once everything above exists
};

}  // namespace mandelbrotter::app
