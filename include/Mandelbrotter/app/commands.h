#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mandelbrotter::app
{

/// What a menu item does. AppController::runCommand runs the ones that need no toolkit; the
/// toolkit runs the others (dialogs, the clipboard, the window and its panel). The names avoid the
/// Windows headers' macros (winuser.h defines HELP_CONTENTS, for instance).
enum class Command : std::uint8_t
{
    SAVE_IMAGE,
    COPY_IMAGE,
    EXPORT_VIEW,
    IMPORT_VIEW,
    QUIT,
    SHOW_PANEL,
    SHOW_ORBIT,
    ZOOM_IN,
    ZOOM_OUT,
    RESET_VIEW,
    ADD_BOOKMARK,
    CONTENTS,
    CONTEXT_HELP,
    REFERENCE,
    FLIGHT,  ///< one per built-in flight, see MenuEntry::flight
    STOP_DEMO,
    TOUR,
    BACK_TO_SNAPSHOT,
    ABOUT,
};

/// Items a platform places itself (macOS moves Quit and About into the application menu).
enum class MenuRole : std::uint8_t
{
    NONE,
    QUIT,
    ABOUT,
    CONTENTS,
};

/// When an item is enabled; everything but ALWAYS asks AppController::commandEnabled.
enum class EnableRule : std::uint8_t
{
    ALWAYS,
    WHILE_DEMO,     ///< a flight or the tour runs
    WITH_SNAPSHOT,  ///< there is a view to go back to
};

struct MenuEntry
{
    enum class Kind : std::uint8_t
    {
        ITEM,
        CHECK,
        SEPARATOR,
        SUBMENU,
    };

    Kind        kind{Kind::ITEM};
    Command     command{};
    std::string label;      ///< with the '&' mnemonic, e.g. "&Save image as PNG..."
    std::string shortcut;   ///< portable text: "Ctrl+S", "Ctrl++", "Ctrl+Home", "F1"; empty: none
    std::string statusTip;  ///< shown in the status bar while the item is hovered
    bool        checked{};  ///< CHECK: the initial state
    MenuRole    role{MenuRole::NONE};
    EnableRule  enable{EnableRule::ALWAYS};
    std::size_t flight{};             ///< FLIGHT: the index into builtinFlights()
    std::vector<MenuEntry> children;  ///< SUBMENU
};

struct Menu
{
    std::string            title;  ///< with the mnemonic: "&File"
    std::vector<MenuEntry> entries;
};

/// The menu bar: File, View, Bookmarks and Help, exactly as the help book documents them
/// (reference.html). Help > Demos holds one item per builtinFlights() entry.
[[nodiscard]] std::vector<Menu> menuBar();

}  // namespace mandelbrotter::app
