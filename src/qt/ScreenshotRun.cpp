#include "qt/ScreenshotRun.h"

#include <QColor>
#include <QDockWidget>
#include <QImage>
#include <QInputDialog>
#include <QMenuBar>
#include <QMetaObject>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QScrollBar>
#include <QStatusBar>
#include <QTimer>
#include <QWidget>
#include <algorithm>
#include <expected>
#include <filesystem>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "Mandelbrotter/app/ScreenshotScript.h"
#include "Mandelbrotter/app/ui_text.h"
#include "qt/FractalCanvas.h"
#include "qt/HelpWindow.h"
#include "qt/MainWindow.h"
#include "qt/SidePanel.h"
#include "qt/qt_util.h"

namespace mandelbrotter::qt
{

namespace
{

/// A picture that is nothing but black (or nothing at all): the capture did not work.
bool isBlank(const QImage& image)
{
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QColor color = image.pixelColor(x, y);
            if (color.alpha() != 0 && (color.red() != 0 || color.green() != 0 || color.blue() != 0))
            {
                return false;
            }
        }
    }
    return true;
}

/// `widget`'s rectangle in the coordinates of its ancestor `target`.
QRect rectIn(const QWidget& widget, const QWidget& target)
{
    return {widget.mapTo(&target, QPoint(0, 0)), widget.size()};
}

}  // namespace

ScreenshotRun::ScreenshotRun(MainWindow& window, std::filesystem::path dir)
  : m_window(window), m_script(window.app(), std::move(dir), makeHooks())
{
    m_settle.setSingleShot(true);
    m_watchdog.setSingleShot(true);
    connect(&m_settle, &QTimer::timeout, this, [this] { m_script.settled(); });
    connect(&m_watchdog, &QTimer::timeout, this, [this] { m_script.watchdogFired(); });
}

ScreenshotRun::~ScreenshotRun()
{
    closeBookmarkDialog();
}

void ScreenshotRun::start()
{
    m_script.start();
}

app::ScreenshotScript::Hooks ScreenshotRun::makeHooks()
{
    return {
        .setContentSize =
            [this](PixelSize size) {
                // wx's SetClientSize: the menu and status bars come on top.
                m_window.resize(size.width, size.height + m_window.menuBar()->height() +
                                                m_window.statusBar()->height());
            },
        .growToWholePanel = [this] { growToWholePanel(); },
        .scrollPanelTo =
            [this](app::PanelSection section) { m_window.panel().scrollToSection(section); },
        .showBookmarkDialog =
            [this](std::string_view text) {
                closeBookmarkDialog();
                auto* dialog = new QInputDialog(&m_window);
                dialog->setWindowTitle(toQt(app::kAddBookmarkTitle));
                dialog->setLabelText(toQt(app::kAddBookmarkPrompt));
                dialog->setTextValue(toQt(text));
                m_dialog = dialog;
                dialog->show();
            },
        .closeBookmarkDialog = [this] { closeBookmarkDialog(); },
        .showHelpContents    = [this] { m_window.help().showContents(); },
        .closeHelp =
            [this] {
                if (m_window.helpShown())
                {
                    m_window.help().close();
                }
            },
        .refreshAll =
            [this] {
                m_window.update();
                for (QWidget* child : m_window.findChildren<QWidget*>())
                {
                    child->update();
                }
            },
        .capture = [this](
                       app::ShotTarget target, app::ShotRegion region,
                       const std::filesystem::path& path) { return capture(target, region, path); },
        .startSettleTimer = [this](std::chrono::milliseconds delay) { m_settle.start(delay); },
        .startWatchdog    = [this](std::chrono::milliseconds delay) { m_watchdog.start(delay); },
        .stopTimers =
            [this] {
                m_settle.stop();
                m_watchdog.stop();
            },
        .post =
            [this](std::function<void()> work) {
                QMetaObject::invokeMethod(this, std::move(work), Qt::QueuedConnection);
            },
        .finished =
            [this](int exitCode) {
                m_exitCode = exitCode;
                QMetaObject::invokeMethod(
                    &m_window, [this] { m_window.close(); }, Qt::QueuedConnection);
            },
        .print      = [](std::string_view line) { std::cout << line << '\n'
                                                            << std::flush; },
        .printError = [](std::string_view line) { std::cerr << line << '\n'; },
    };
}

void ScreenshotRun::growToWholePanel()
{
    // Tall enough for the whole panel (normally it scrolls), its dock's title and the slack.
    const SidePanel& panel    = m_window.panel();
    const int        overhead = m_window.panelDock().height() - panel.viewport()->height();
    const int content = std::max(app::kScreenshotContentSize.height,
                                 panel.contentHeight() + overhead + app::kScreenshotPanelSlack);
    m_window.resize(app::kScreenshotContentSize.width,
                    content + m_window.menuBar()->height() + m_window.statusBar()->height());
}

void ScreenshotRun::closeBookmarkDialog()
{
    if (QInputDialog* dialog = m_dialog.data(); dialog != nullptr)
    {
        m_dialog = nullptr;
        dialog->hide();
        dialog->deleteLater();
    }
}

QWidget* ScreenshotRun::targetWindow(app::ShotTarget target) const
{
    switch (target)
    {
        case app::ShotTarget::MAIN:
            return &m_window;
        case app::ShotTarget::EXPORT_DIALOG:
            return m_window.exportDialog();
        case app::ShotTarget::BOOKMARK_DIALOG:
            return m_dialog.data();
        case app::ShotTarget::HELP:
            return m_window.helpShown() ? &m_window.help() : nullptr;
    }
    return nullptr;
}

std::optional<QRect> ScreenshotRun::regionRect(app::ShotRegion region, const QWidget& target) const
{
    switch (region.kind)
    {
        case app::ShotRegion::Kind::WHOLE:
            break;
        case app::ShotRegion::Kind::PANEL:
            return rectIn(m_window.panel(), target);
        case app::ShotRegion::Kind::SECTION:
            return m_window.panel().sectionRect(region.section, target);
        case app::ShotRegion::Kind::CANVAS:
            return rectIn(m_window.canvas(), target);
        case app::ShotRegion::Kind::STATUS_BAR:
            return rectIn(*m_window.statusBar(), target);
    }
    return std::nullopt;
}

std::expected<PixelSize, std::string> ScreenshotRun::capture(app::ShotTarget              target,
                                                             app::ShotRegion              region,
                                                             const std::filesystem::path& path)
{
    QWidget* widget = targetWindow(target);
    if (widget == nullptr)
    {
        return std::unexpected("the window to capture does not exist");
    }
    widget->raise();
    const QPixmap shot  = widget->grab();
    QImage        image = shot.toImage();
    if (image.isNull() || isBlank(image))
    {
        return std::unexpected("the capture is empty");
    }
    const double ratio = shot.devicePixelRatio();
    if (const std::optional<QRect> rect = regionRect(region, *widget))
    {
        const QRect keep = rect->adjusted(-app::kScreenshotMarginPx, -app::kScreenshotMarginPx,
                                          app::kScreenshotMarginPx, app::kScreenshotMarginPx)
                               .intersected(widget->rect());
        if (keep.isEmpty())
        {
            return std::unexpected("the region to keep lies outside the captured window");
        }
        image = image.copy(QRect((keep.topLeft().toPointF() * ratio).toPoint(),
                                 (keep.size().toSizeF() * ratio).toSize()));
    }
    // Back to logical pixels, and no wider than a help page shows.
    QSize size = (image.size().toSizeF() / ratio).toSize();
    if (size.width() > app::kScreenshotMaxWidth)
    {
        size = QSize(app::kScreenshotMaxWidth,
                     std::max(1, size.height() * app::kScreenshotMaxWidth / size.width()));
    }
    if (size != image.size())
    {
        image = image.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    image.setDevicePixelRatio(1.0);
    if (!image.save(fromPath(path), "PNG"))
    {
        return std::unexpected("could not write " + path.string());
    }
    return PixelSize{image.width(), image.height()};
}

}  // namespace mandelbrotter::qt
