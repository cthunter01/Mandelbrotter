#pragma once

#include <QEvent>
#include <QKeyEvent>
#include <QObject>
#include <QWidget>
#include <functional>

#include "Mandelbrotter/app/AppController.h"

namespace mandelbrotter::qt
{

/// The main window's keys before any control sees them (wx: the frame's char hook). Installed on
/// the application, it acts only on key events for the main window's focus widget (or the window
/// itself when nothing there has the focus), so the help window and the dialogs are left alone:
/// - Esc goes to the application (AppController::handleGlobalKey), which stops a demo and then
///   consumes it;
/// - every other key ends a flight and then does its usual job, a menu shortcut included (it never
///   produces a key press, so it is caught as a ShortcutOverride);
/// - a menu shortcut beats the focused control: a line edit or spin box would otherwise claim
///   Ctrl+C or Ctrl+Home for itself.
class GlobalKeyFilter : public QObject
{
public:
    struct Hooks
    {
        /// AppController::handleGlobalKey: true when the key was consumed.
        std::function<bool(app::GlobalKey)> key;
        /// True when the key event is the shortcut of one of the main window's menu items.
        std::function<bool(const QKeyEvent&)> isMenuShortcut;
    };

    GlobalKeyFilter(QWidget& window, Hooks hooks);

    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// True when `watched` is the main window's focus widget, or the window when none has it.
    [[nodiscard]] bool isWindowsFocus(const QObject* watched) const;

    QWidget& m_window;
    Hooks    m_hooks;
};

}  // namespace mandelbrotter::qt
