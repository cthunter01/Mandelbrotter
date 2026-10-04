#include "Mandelbrotter/app/commands.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "Mandelbrotter/flights.h"

namespace mandelbrotter::app
{

namespace
{

using Kind = MenuEntry::Kind;

MenuEntry item(Command command, std::string label, std::string shortcut = {},
               std::string statusTip = {})
{
    MenuEntry entry;
    entry.command   = command;
    entry.label     = std::move(label);
    entry.shortcut  = std::move(shortcut);
    entry.statusTip = std::move(statusTip);
    return entry;
}

MenuEntry check(Command command, std::string label, std::string shortcut, bool checked)
{
    MenuEntry entry = item(command, std::move(label), std::move(shortcut));
    entry.kind      = Kind::CHECK;
    entry.checked   = checked;
    return entry;
}

MenuEntry separator()
{
    MenuEntry entry;
    entry.kind = Kind::SEPARATOR;
    return entry;
}

MenuEntry withRole(MenuEntry entry, MenuRole role)
{
    entry.role = role;
    return entry;
}

MenuEntry enabledWhen(MenuEntry entry, EnableRule rule)
{
    entry.enable = rule;
    return entry;
}

MenuEntry demosMenu()
{
    MenuEntry demos    = item(Command{}, "&Demos", {}, "Animated dives into famous places");
    demos.kind         = Kind::SUBMENU;
    const auto flights = builtinFlights();
    for (std::size_t i = 0; i < flights.size(); ++i)
    {
        MenuEntry entry = item(Command::FLIGHT, flights[i].title, {}, flights[i].description);
        entry.flight    = i;
        demos.children.push_back(std::move(entry));
    }
    demos.children.push_back(separator());
    demos.children.push_back(
        enabledWhen(item(Command::STOP_DEMO, "&Stop demo", {}, "Stop the flight or the tour"),
                    EnableRule::WHILE_DEMO));
    return demos;
}

}  // namespace

std::vector<Menu> menuBar()
{
    return {
        {
            .title = "&File",
            .entries =
                {
                    item(Command::SAVE_IMAGE, "&Save image as PNG...", "Ctrl+S"),
                    item(Command::COPY_IMAGE, "&Copy image", "Ctrl+C"),
                    separator(),
                    item(Command::EXPORT_VIEW, "&Export view...", {},
                         "Save the current view as a JSON file"),
                    item(Command::IMPORT_VIEW, "&Import view...", {}, "Open a view saved as JSON"),
                    separator(),
                    withRole(item(Command::QUIT, "&Quit", "Ctrl+Q"), MenuRole::QUIT),
                },
        },
        {
            .title = "&View",
            .entries =
                {
                    check(Command::SHOW_PANEL, "Show &side panel", "Ctrl+B", true),
                    check(Command::SHOW_ORBIT, "Show &orbit under cursor", "Ctrl+O", false),
                    separator(),
                    item(Command::ZOOM_IN, "Zoom &in", "Ctrl++"),
                    item(Command::ZOOM_OUT, "Zoom &out", "Ctrl+-"),
                    item(Command::RESET_VIEW, "&Reset view", "Ctrl+Home"),
                },
        },
        {
            .title   = "&Bookmarks",
            .entries = {item(Command::ADD_BOOKMARK, "&Add bookmark...", "Ctrl+D")},
        },
        {
            .title = "&Help",
            .entries =
                {
                    withRole(item(Command::CONTENTS, "&Contents", {}, "Open the user guide"),
                             MenuRole::CONTENTS),
                    item(Command::CONTEXT_HELP, "Help for the &focused control", "F1",
                         "Open the page about the control that has the keyboard focus"),
                    item(Command::REFERENCE, "Keyboard and mouse &reference"),
                    separator(),
                    demosMenu(),
                    item(Command::TOUR, "Take a &tour", {},
                         "A guided walk through the window, step by step"),
                    enabledWhen(item(Command::BACK_TO_SNAPSHOT, "&Back to where I was", {},
                                     "Return to the view from before the last demo"),
                                EnableRule::WITH_SNAPSHOT),
                    separator(),
                    withRole(item(Command::ABOUT, "&About Mandelbrotter"), MenuRole::ABOUT),
                },
        },
    };
}

}  // namespace mandelbrotter::app
