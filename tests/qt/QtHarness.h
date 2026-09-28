#pragma once

#include <QCoreApplication>
#include <QString>
#include <QTest>
#include <chrono>
#include <functional>
#include <string>

#include <gtest/gtest.h>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/scenes.h"
#include "TempDir.h"
#include "qt/ElidedLabel.h"
#include "qt/MainWindow.h"
#include "qt/qt_util.h"

namespace mandelbrotter::test
{

/// Runs Qt's event loop until `done` holds; false after `timeout`.
[[nodiscard]] inline bool pumpUntil(const std::function<bool()>& done,
                                    std::chrono::milliseconds    timeout = std::chrono::seconds(10))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!done())
    {
        if (std::chrono::steady_clock::now() > deadline)
        {
            return false;
        }
        QCoreApplication::processEvents();
        QTest::qWait(5);
    }
    return true;
}

/// A main window, shown, with its bookmarks in a fresh directory.
struct QtHarness
{
    explicit QtHarness(const RenderSettings& initial = app::mandelbrotDefault())
      : window(initial, dir / "bookmarks.json")
    {
        window.show();
    }

    [[nodiscard]] std::string status(app::StatusField field) const
    {
        return qt::fromQt(window.statusField(field).fullText());
    }
    /// The canvas has finished rendering its picture.
    [[nodiscard]] bool renderDone() const
    {
        return status(app::StatusField::RENDER).starts_with("Rendered");
    }

    TempDir        dir;
    qt::MainWindow window;
};

}  // namespace mandelbrotter::test
