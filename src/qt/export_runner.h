#pragma once

#include <QWidget>
#include <filesystem>
#include <functional>
#include <string_view>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/exporter.h"

namespace mandelbrotter::qt
{

/// Renders and writes a PNG on a worker thread behind a cancelable, application-modal progress
/// dialog. A failure (not a cancel) goes to `reportError`. Returns true if the file was written.
bool exportPngWithProgress(
    QWidget* parent, const RenderSettings& settings, const ExportOptions& options,
    const std::filesystem::path&                                                 path,
    const std::function<void(std::string_view title, std::string_view message)>& reportError);

}  // namespace mandelbrotter::qt
