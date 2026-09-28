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
#include "Mandelbrotter/app/TourScript.h"
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
    /// One showCard call of the tour.
    struct Card
    {
        std::string     title;
        std::size_t     index{};
        std::size_t     count{};
        bool            hasHelpPage{};
        app::TourTarget anchor{};
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
            .now  = [this] { return clock; },
            .tour = {.showCard =
                         [this](const app::TourStep& step, std::size_t index, std::size_t count) {
                             ++hookCalls;
                             cards.push_back({.title       = step.title,
                                              .index       = index,
                                              .count       = count,
                                              .hasHelpPage = !step.helpPage.empty(),
                                              .anchor      = step.anchor});
                             cardShown = true;
                         },
                     .hideCard =
                         [this] {
                             ++hookCalls;
                             cardShown = false;
                         },
                     .highlight =
                         [this](std::optional<app::TourTarget> target) {
                             ++hookCalls;
                             highlights.push_back(target);
                         }},
        };
    }

    std::chrono::steady_clock::time_point           clock;
    std::vector<Error>                              errors;
    std::vector<RenderSettings>                     panelSettings;
    std::vector<Bookmark>                           bookmarks;
    std::vector<std::optional<Complex>>             previewSeeds;
    std::vector<std::string>                        helpPages;
    std::vector<Card>                               cards;
    std::vector<std::optional<app::TourTarget>>     highlights;
    std::array<std::string, app::kStatusFieldCount> statusFields;
    int                                             effectiveIterations{0};
    int                                             exportDialogShows{0};
    int                                             raises{0};
    int                                             hookCalls{0};
    bool                                            exportDialogOpen{false};
    bool                                            demoTimer{false};
    bool                                            cardShown{false};
    std::optional<bool>                             pickSeedMode;
    std::optional<bool>                             showOrbit;
};

/// An application on a 40 x 30 canvas whose renders post to `queue`, with its bookmarks in a
/// fresh directory (or in its subdirectory `bookmarksDir`). start() is left to the test.
struct Harness
{
    static constexpr PixelSize kCanvasSize{40, 30};

    explicit Harness(const RenderSettings& initial      = app::mandelbrotDefault(),
                     const std::string&    bookmarksDir = {})
      : bookmarks(bookmarksDir.empty() ? dir / "bookmarks.json"
                                       : dir.path() / bookmarksDir / "bookmarks.json"),
        canvas(initial,
               {.post = queue.hook(), .requestRepaint = {}, .setCursor = {}, .captureMouse = {}}),
        app(initial, bookmarks, canvas, shell.shell())
    {
        canvas.setSize(kCanvasSize, 1.0);
    }

    [[nodiscard]] std::filesystem::path bookmarksFile() const { return bookmarks; }
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
    std::filesystem::path bookmarks;
    PostQueue             queue;
    app::CanvasController canvas;
    RecordingShell        shell;
    app::AppController    app;  ///< last: destroyed first
};

}  // namespace mandelbrotter::test
