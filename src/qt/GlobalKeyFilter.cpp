#include "qt/GlobalKeyFilter.h"

#include <QEvent>
#include <QKeyEvent>
#include <QObject>
#include <QWidget>
#include <utility>

#include "Mandelbrotter/app/AppController.h"

namespace mandelbrotter::qt
{

GlobalKeyFilter::GlobalKeyFilter(QWidget& window, Hooks hooks)
  : QObject(&window), m_window(window), m_hooks(std::move(hooks))
{
}

bool GlobalKeyFilter::isWindowsFocus(const QObject* watched) const
{
    const QWidget* focus = m_window.focusWidget();
    return watched == (focus != nullptr ? focus : &m_window);
}

bool GlobalKeyFilter::eventFilter(QObject* watched, QEvent* event)
{
    const QEvent::Type type = event->type();
    if ((type != QEvent::KeyPress && type != QEvent::ShortcutOverride) ||
        !isWindowsFocus(watched) || !m_hooks.key)
    {
        return false;
    }
    const auto* key    = static_cast<QKeyEvent*>(event);
    const bool  escape = key->key() == Qt::Key_Escape;
    if (type == QEvent::KeyPress)
    {
        return m_hooks.key(escape ? app::GlobalKey::ESCAPE : app::GlobalKey::OTHER);
    }
    // ShortcutOverride: sent first for every key press. Esc waits for its key press.
    if (escape)
    {
        return false;
    }
    m_hooks.key(app::GlobalKey::OTHER);
    if (m_hooks.isMenuShortcut && m_hooks.isMenuShortcut(*key))
    {
        // Not accepted, and kept from the focused control: the shortcut runs.
        event->ignore();
        return true;
    }
    return false;
}

}  // namespace mandelbrotter::qt
