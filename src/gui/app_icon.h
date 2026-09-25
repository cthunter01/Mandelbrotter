#pragma once

#include <span>
#include <string_view>

#include <wx/toplevel.h>

namespace mandelbrotter::gui
{

/// Gives a top-level window the application icon (src/icons) for its title bar and taskbar entry.
/// Does nothing on macOS, which takes an application's icon from its .app bundle only.
void applyAppIcon(wxTopLevelWindow& window);

/// The 256 px icon PNG in pieces, GTK builds only: generated into the build tree by
/// Mandelbrotter_embed (src/CMakeLists.txt). Windows reads the icon from the executable's resources
/// instead.
[[nodiscard]] std::span<const std::string_view> appIconPngChunks() noexcept;

}  // namespace mandelbrotter::gui
