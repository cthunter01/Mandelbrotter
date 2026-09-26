#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/CanvasController.h"
#include "Mandelbrotter/app/scenes.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/geometry.h"
#include "PostQueue.h"
#include "TempDir.h"

namespace mandelbrotter::test
{

/// A toolkit that records what the application asks of it (app::AppController::Shell).
struct RecordingShell
{
    struct Error
    {
        std::string title;
        std::string message;
    };

    [[nodiscard]] const std::string& status(app::StatusField field) const
    {
        return statusFields.at(static_cast<std::size_t>(field));
    }
    void advance(std::chrono::milliseconds by) { clock += by; }
    /// How many hooks have been called so far.
    [[nodiscard]] int calls() const { return hookCalls; }

    [[nodiscard]] app::AppController::Shell shell()
    {
        return {
            .setStatus =
                [this](app::StatusField field, std::string_view text) {
                    ++hookCalls;
                    statusFields.at(static_cast<std::size_t>(field)) = std::string(text);
                },
            .reportError =
                [this](std::string_view title, std::string_view message) {
                    ++hookCalls;
                    errors.push_back(
                        {.title = std::string(title), .message = std::string(message)});
                },
            .panelSettings =
                [this](const RenderSettings& settings) {
                    ++hookCalls;
                    panelSettings.push_back(settings);
                },
            .panelEffectiveIterations =
                [this](int iterations) {
                    ++hookCalls;
                    effectiveIterations = iterations;
                },
            .panelBookmarks =
                [this](std::span<const Bookmark> list) {
                    ++hookCalls;
                    bookmarks.assign(list.begin(), list.end());
                },
            .panelPickSeedMode =
                [this](bool on) {
                    ++hookCalls;
                    pickSeedMode = on;
                },
            .showOrbitChanged =
                [this](bool on) {
                    ++hookCalls;
                    showOrbit = on;
                },
            .panelPreviewSeed =
                [this](std::optional<Complex> seed) {
                    ++hookCalls;
                    previewSeeds.push_back(seed);
                },
            .showExportDialog =
                [this] {
                    ++hookCalls;
                    exportDialogOpen = true;
                    ++exportDialogShows;
                },
            .closeExportDialog =
                [this] {
                    ++hookCalls;
                    exportDialogOpen = false;
                },
            .raiseWindow =
                [this] {
                    ++hookCalls;
                    ++raises;
                },
            .showHelpPage =
                [this](std::string_view page) {
                    ++hookCalls;
                    helpPages.emplace_back(page);
                },
            .demoTimer =
                [this](bool on) {
                    ++hookCalls;
                    demoTimer = on;
                },
            .now = [this] { return clock; },
            .startTour =
                [this] {
                    ++hookCalls;
                    tourRunning = true;
                    ++tourStarts;
                },
            .stopTour =
                [this] {
                    ++hookCalls;
                    tourRunning = false;
                },
            .tourRunning = [this] { return tourRunning; },
        };
    }

    std::array<std::string, app::kStatusFieldCount> statusFields;
    std::vector<Error>                              errors;
    std::vector<RenderSettings>                     panelSettings;
    int                                             effectiveIterations{0};
    std::vector<Bookmark>                           bookmarks;
    std::optional<bool>                             pickSeedMode;
    std::optional<bool>                             showOrbit;
    std::vector<std::optional<Complex>>             previewSeeds;
    bool                                            exportDialogOpen{false};
    int                                             exportDialogShows{0};
    int                                             raises{0};
    std::vector<std::string>                        helpPages;
    bool                                            demoTimer{false};
    std::chrono::steady_clock::time_point           clock;
    bool                                            tourRunning{false};
    int                                             tourStarts{0};
    int                                             hookCalls{0};
};

/// An application on a 40 x 30 canvas whose renders post to `queue`, with its bookmarks in a
/// fresh directory. start() is left to the test.
struct Harness
{
    static constexpr PixelSize kCanvasSize{40, 30};

    explicit Harness(const RenderSettings& initial = app::mandelbrotDefault())
      : canvas(initial,
               {.post = queue.hook(), .requestRepaint = {}, .setCursor = {}, .captureMouse = {}}),
        app(initial, bookmarksFile(), canvas, shell.shell())
    {
        canvas.setSize(kCanvasSize, 1.0);
    }

    [[nodiscard]] std::filesystem::path bookmarksFile() const { return dir / "bookmarks.json"; }
    /// Runs the posted render work until the status bar says the render in progress finished.
    [[nodiscard]] bool finishRender()
    {
        return queue.waitAndDrainUntil(
            [this] { return shell.status(app::StatusField::RENDER).starts_with("Rendered"); });
    }
    /// Runs the posted render work until the canvas shows its coarse pass (a flight's next frame
    /// may come).
    [[nodiscard]] bool coarsePicture()
    {
        return queue.waitAndDrainUntil([this] { return canvas.hasCoarsePicture(); });
    }

    TempDir               dir;
    PostQueue             queue;
    app::CanvasController canvas;
    RecordingShell        shell;
    app::AppController    app;  ///< last: destroyed first
};

}  // namespace mandelbrotter::test
