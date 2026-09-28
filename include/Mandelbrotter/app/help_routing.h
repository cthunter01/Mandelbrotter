#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "Mandelbrotter/app/panel_model.h"

namespace mandelbrotter::app
{

/// Pages of the help book the views open by name.
inline constexpr std::string_view kContentsPage  = "index.html";
inline constexpr std::string_view kReferencePage = "reference.html";  ///< Help > Keyboard and mouse
inline constexpr std::string_view kExportingPage = "exporting.html";  ///< F1 in the export dialog

/// Where the keyboard focus is when the user asks for help (F1).
struct HelpContext
{
    enum class Area : std::uint8_t
    {
        NONE,  ///< nothing of ours has the focus
        CANVAS,
        PANEL,
        EXPORT_DIALOG,
    };

    Area                        area{Area::NONE};
    std::optional<PanelSection> section;         ///< with PANEL: the box holding the control
    bool                        juliaControl{};  ///< with PANEL: one of the Julia controls
};

/// The help book page about the focused control ("navigating.html"); "index.html" when there is
/// none.
[[nodiscard]] std::string_view helpPageFor(const HelpContext& context) noexcept;

/// What a link in the help book leads to.
enum class HelpLinkKind : std::uint8_t
{
    ACTION,    ///< "mandelbrotter:..." acts on the main window (help_action.h)
    EXTERNAL,  ///< http(s): the default browser
    PAGE,      ///< anything else is a page of the book
};

[[nodiscard]] HelpLinkKind classifyHelpLink(std::string_view url) noexcept;

}  // namespace mandelbrotter::app
