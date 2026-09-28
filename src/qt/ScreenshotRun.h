#pragma once

#include <QInputDialog>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QTimer>
#include <QWidget>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>

#include "Mandelbrotter/app/ScreenshotScript.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::qt
{

class MainWindow;

/// The developer screenshot mode (--screenshots DIR) on Qt: app::ScreenshotScript walks the window
/// through the states the help book shows; this fills its hooks with Qt's timers, the dialogs and
/// QWidget::grab(), which renders a widget into a picture by itself (offscreen too, and on
/// Wayland), and closes the window when the script is done.
class ScreenshotRun : public QObject
{
public:
    ScreenshotRun(MainWindow& window, std::filesystem::path dir);
    ~ScreenshotRun() override;
    ScreenshotRun(const ScreenshotRun&)            = delete;
    ScreenshotRun& operator=(const ScreenshotRun&) = delete;
    ScreenshotRun(ScreenshotRun&&)                 = delete;
    ScreenshotRun& operator=(ScreenshotRun&&)      = delete;

    void               start();
    [[nodiscard]] bool done() const noexcept { return m_script.done(); }
    /// The process exit code once done: 0, or 1 after a failure.
    [[nodiscard]] int exitCode() const noexcept { return m_exitCode; }

private:
    [[nodiscard]] app::ScreenshotScript::Hooks makeHooks();
    [[nodiscard]] QWidget*                     targetWindow(app::ShotTarget target) const;
    /// The region in the target's coordinates; nullopt for the whole target.
    [[nodiscard]] std::optional<QRect>                  regionRect(app::ShotRegion region,
                                                                   const QWidget&  target) const;
    [[nodiscard]] std::expected<PixelSize, std::string> capture(app::ShotTarget              target,
                                                                app::ShotRegion              region,
                                                                const std::filesystem::path& path);
    void                                                growToWholePanel();
    void                                                closeBookmarkDialog();

    MainWindow&            m_window;
    QTimer                 m_settle;
    QTimer                 m_watchdog;
    QPointer<QInputDialog> m_dialog;  ///< the Add bookmark dialog of its shot
    int                    m_exitCode{0};
    app::ScreenshotScript  m_script;  ///< last: its hooks use the members above
};

}  // namespace mandelbrotter::qt
