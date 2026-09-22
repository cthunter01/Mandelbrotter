#pragma once

#include <filesystem>

#include <wx/window.h>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/exporter.h"

namespace mandelbrotter::gui
{

/// Renders and writes a PNG on a worker thread behind a cancellable progress dialog. Reports errors
/// to the user. Returns true if the file was written.
bool exportPngWithProgress(wxWindow* parent, const RenderSettings& settings,
                           const ExportOptions& options, const std::filesystem::path& path);

}  // namespace mandelbrotter::gui
