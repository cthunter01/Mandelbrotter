#pragma once

#include <span>
#include <string_view>

namespace mandelbrotter::gui
{

/// The help book (docs/help, zipped at build time) in pieces small enough for every compiler's
/// string literal limits. The definition is generated into the build tree by Mandelbrotter_embed
/// (cmake/HelpBook.cmake).
[[nodiscard]] std::span<const std::string_view> helpBookChunks() noexcept;

/// The whole zip, assembled on first use.
[[nodiscard]] std::string_view helpBookZip();

}  // namespace mandelbrotter::gui
