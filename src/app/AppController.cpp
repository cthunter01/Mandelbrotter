#include "Mandelbrotter/app/AppController.h"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/ProgressiveRenderer.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/Viewport.h"
#include "Mandelbrotter/app/CanvasController.h"
#include "Mandelbrotter/app/DemoPlayer.h"
#include "Mandelbrotter/app/TourScript.h"
#include "Mandelbrotter/app/commands.h"
#include "Mandelbrotter/app/format.h"
#include "Mandelbrotter/bookmarks.h"
#include "Mandelbrotter/flights.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"
#include "Mandelbrotter/help_action.h"

namespace mandelbrotter::app
{

namespace
{

/// Calls a Shell hook that may be empty.
template <typename Signature, typename... Args>
void call(const std::function<Signature>& hook, Args&&... args)
{
    if (hook)
    {
        hook(std::forward<Args>(args)...);
    }
}

}  // namespace

AppController::AppController(RenderSettings initial, std::filesystem::path bookmarksPath,
                             CanvasController& canvas, Shell shell)
  : m_settings(std::move(initial)),
    m_bookmarks(std::move(bookmarksPath)),
    m_canvas(canvas),
    m_shell(std::move(shell)),
    m_demo(demoHooks())
{
    wireCanvas();
}

AppController::~AppController()
{
    m_canvas.onViewChanged  = nullptr;
    m_canvas.onPointerMoved = nullptr;
    m_canvas.onSeedPicked   = nullptr;
    m_canvas.onRenderStatus = nullptr;
    m_canvas.onUserInput    = nullptr;
}

DemoPlayer::Hooks AppController::demoHooks()
{
    return {.applyFrame =
                [this](const RenderSettings& frame) {
                    m_settings = frame;
                    m_canvas.setSettings(frame);
                    updateStatusBar();  // the panel catches up when the flight ends
                },
            .canvasReady = [this] { return m_canvas.hasCoarsePicture(); },
            .showStatus  = [this](std::string_view text) { setStatus(StatusField::POINTER, text); },
            .finished    = [this] { call(m_shell.panelSettings, m_settings); },
            .setTimerRunning = [this](bool on) { call(m_shell.demoTimer, on); },
            .now             = m_shell.now};
}

void AppController::wireCanvas()
{
    m_canvas.onUserInput   = [this] { stopFlight(); };
    m_canvas.onViewChanged = [this](const ViewSpec& view) {
        stopFlight();
        m_settings.view = view;
        call(m_shell.panelSettings, m_settings);
        updateStatusBar();
    };
    m_canvas.onPointerMoved = [this](const std::optional<BigComplex>& pointer) {
        showPointer(pointer);
        setPreviewSeed(pointer ? std::optional<Complex>(pointer->approx())
                               : std::optional<Complex>());
    };
    m_canvas.onSeedPicked = [this](Complex seed) {
        RenderSettings next = m_settings;
        next.fractal.julia  = true;
        next.fractal.seed   = seed;
        next.view           = defaultView(next.fractal);
        setPickSeedMode(false);
        applySettings(next);
    };
    m_canvas.onRenderStatus = [this](const RenderStatus& status) { showRenderStatus(status); };
}

void AppController::start()
{
    if (const std::string error = m_bookmarks.load(); !error.empty())
    {
        reportError("Bookmarks", "Could not read " + m_bookmarks.path().string() + ":\n" + error);
    }
    refreshBookmarks();
    applySettings(m_settings);
}

// ---------------------------------------------------------------------------------------------------------------
// The model

void AppController::applySettings(const RenderSettings& settings)
{
    m_settings = settings;
    m_canvas.setSettings(settings);
    call(m_shell.panelSettings, settings);
    updateStatusBar();
}

void AppController::setShowOrbit(bool on)
{
    m_canvas.setShowOrbit(on);
    call(m_shell.showOrbitChanged, on);
}

void AppController::setPickSeedMode(bool on)
{
    m_canvas.setPickSeedMode(on);
    call(m_shell.panelPickSeedMode, on);
}

void AppController::setPreviewSeed(std::optional<Complex> seed) const
{
    call(m_shell.panelPreviewSeed, seed);
}

void AppController::panelEdited(const RenderSettings& edited)
{
    stopFlight();
    RenderSettings next = edited;
    next.view           = m_settings.view;  // the panel never edits the view
    if (next.fractal.family != m_settings.fractal.family ||
        next.fractal.julia != m_settings.fractal.julia)
    {
        next.view = defaultView(next.fractal);
    }
    applySettings(next);
}

// ---------------------------------------------------------------------------------------------------------------
// Status bar

void AppController::setStatus(StatusField field, std::string_view text) const
{
    call(m_shell.setStatus, field, text);
}

void AppController::reportError(std::string_view title, std::string_view message) const
{
    call(m_shell.reportError, title, message);
}

void AppController::updateStatusBar()
{
    const ViewSpec& view = m_settings.view;
    setStatus(StatusField::CENTER, "Center " + formatCenter(view.center, view.zoom));
    const std::string zoomText =
        formatZoom(view.zoom) + (usesPerturbation(view.zoom) ? " (deep)" : "");  // perturbation
    setStatus(StatusField::ZOOM, "Zoom " + zoomText);
    setStatus(StatusField::ITERATIONS,
              std::format("{} iterations", effectiveIterations(m_settings)));
    call(m_shell.panelEffectiveIterations, effectiveIterations(m_settings));
}

void AppController::showPointer(const std::optional<BigComplex>& pointer)
{
    if (m_demo.playing())
    {
        return;  // the field shows the flight's progress
    }
    setStatus(StatusField::POINTER,
              pointer ? formatCenter(*pointer, m_settings.view.zoom) : std::string());
}

void AppController::showRenderStatus(const RenderStatus& status)
{
    if (status.rendering)
    {
        setStatus(StatusField::RENDER, "Rendering...");
        return;
    }
    setStatus(StatusField::RENDER, std::format("Rendered in {} ms", status.elapsed.count()));
    if (onRenderFinished)
    {
        onRenderFinished();
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Menus

bool AppController::commandEnabled(Command command) const noexcept
{
    switch (command)
    {
        case Command::STOP_DEMO:
            return flightPlaying() || tourRunning();
        case Command::BACK_TO_SNAPSHOT:
            return hasSnapshot();
        default:
            return true;
    }
}

bool AppController::runCommand(Command command, std::size_t flight)
{
    switch (command)
    {
        case Command::ZOOM_IN:
            zoomIn();
            return true;
        case Command::ZOOM_OUT:
            zoomOut();
            return true;
        case Command::RESET_VIEW:
            resetView();
            return true;
        case Command::FLIGHT:
        {
            const auto flights = builtinFlights();
            if (flight < flights.size())
            {
                takeSnapshot();
                startFlight(flights[flight].id);
            }
            return true;
        }
        case Command::STOP_DEMO:
            stopDemos();
            return true;
        case Command::TOUR:
            takeSnapshot();
            startTour();
            return true;
        case Command::BACK_TO_SNAPSHOT:
            restoreSnapshot();
            return true;
        case Command::SAVE_IMAGE:
        case Command::COPY_IMAGE:
        case Command::EXPORT_VIEW:
        case Command::IMPORT_VIEW:
        case Command::QUIT:
        case Command::SHOW_PANEL:
        case Command::SHOW_ORBIT:
        case Command::ADD_BOOKMARK:
        case Command::CONTENTS:
        case Command::CONTEXT_HELP:
        case Command::REFERENCE:
        case Command::ABOUT:
            break;
    }
    return false;
}

void AppController::zoomIn()
{
    stopFlight();
    m_canvas.zoomAtCenter(kMenuZoomFactor);
}

void AppController::zoomOut()
{
    stopFlight();
    m_canvas.zoomAtCenter(1.0 / kMenuZoomFactor);
}

void AppController::resetView()
{
    stopFlight();
    m_canvas.resetView();
}

void AppController::importView(const std::filesystem::path& path)
{
    try
    {
        stopFlight();
        applySettings(loadView(path));
    }
    catch (const std::exception& e)
    {
        reportError("Import view", e.what());
    }
}

void AppController::exportView(const std::filesystem::path& path)
{
    try
    {
        saveView(path, m_settings);
    }
    catch (const std::exception& e)
    {
        reportError("Export view", e.what());
    }
}

void AppController::imageSaved(const std::filesystem::path& path)
{
    setStatus(StatusField::RENDER, "Saved " + path.filename().string());
}

void AppController::imageCopied()
{
    setStatus(StatusField::RENDER, "Image copied to clipboard");
}

// ---------------------------------------------------------------------------------------------------------------
// Bookmarks

std::string AppController::beginAddBookmark()
{
    stopDemos();  // the tour's temporary bookmark must never be saved
    return defaultBookmarkName(m_settings);
}

void AppController::addBookmark(std::string name)
{
    if (name.empty())
    {
        name = "Untitled";
    }
    m_bookmarks.add({.name = std::move(name), .settings = m_settings});
    saveBookmarksFile();
    refreshBookmarks();
}

void AppController::loadBookmark(std::size_t index)
{
    stopFlight();
    const auto& list = m_bookmarks.bookmarks();
    if (index < list.size())
    {
        applySettings(list[index].settings);
    }
}

void AppController::deleteBookmark(std::size_t index)
{
    if (tourRunning())
    {
        stopDemos();  // removes the tour's example; the list has changed under the selection
        return;
    }
    stopFlight();
    m_bookmarks.remove(index);
    saveBookmarksFile();
    refreshBookmarks();
}

void AppController::saveBookmarksFile() const
{
    if (const std::string error = m_bookmarks.save(); !error.empty())
    {
        reportError("Bookmarks", "Could not write " + m_bookmarks.path().string() + ":\n" + error);
    }
}

void AppController::refreshBookmarks() const
{
    call(m_shell.panelBookmarks, std::span<const Bookmark>(m_bookmarks.bookmarks()));
}

void AppController::addTemporaryBookmark(std::string name)
{
    removeTemporaryBookmark();
    Bookmark bookmark{.name = std::move(name), .settings = m_settings};
    m_temporaryBookmark = bookmark;
    m_bookmarks.add(std::move(bookmark));
    refreshBookmarks();
}

void AppController::removeTemporaryBookmark()
{
    if (!m_temporaryBookmark)
    {
        return;
    }
    const auto& list = m_bookmarks.bookmarks();
    for (std::size_t i = list.size(); i-- > 0;)
    {
        if (list[i] == *m_temporaryBookmark)
        {
            m_bookmarks.remove(i);
            break;
        }
    }
    m_temporaryBookmark.reset();
    refreshBookmarks();
}

// ---------------------------------------------------------------------------------------------------------------
// Help and demos

void AppController::runHelpAction(const HelpAction& action)
{
    if (const auto* view = std::get_if<ViewAction>(&action))
    {
        const auto next = resolveViewAction(m_settings, *view);
        if (!next)
        {
            reportError("Try it", next.error());
            return;
        }
        takeSnapshot();
        stopFlight();
        applySettings(*next);
    }
    else if (const auto* flight = std::get_if<FlightAction>(&action))
    {
        takeSnapshot();
        startFlight(flight->id);
    }
    else if (std::holds_alternative<TourAction>(action))
    {
        takeSnapshot();
        startTour();
    }
    else if (const auto* orbit = std::get_if<OrbitAction>(&action))
    {
        takeSnapshot();
        setShowOrbit(orbit->on);
    }
    else if (std::holds_alternative<ResetAction>(action))
    {
        takeSnapshot();
        stopFlight();
        m_canvas.resetView();
    }
    else if (std::holds_alternative<ExportDialogAction>(action))
    {
        showExportDialog();
    }
    call(m_shell.raiseWindow);
}

void AppController::startFlight(std::string_view id)
{
    const Flight* flight = findFlight(id);
    if (flight == nullptr)
    {
        reportError("Demos", "There is no flight called \"" + std::string(id) + "\".");
        return;
    }
    if (m_tour)
    {
        m_tour->stop();
    }
    m_demo.play(*flight);
}

void AppController::startTour()
{
    stopFlight();
    tour().start();
}

TourScript& AppController::tour()
{
    if (!m_tour)
    {
        m_tour = std::make_unique<TourScript>(*this, m_shell.tour);
    }
    return *m_tour;
}

void AppController::stopFlight()
{
    m_demo.stop();
}

void AppController::stopDemos()
{
    stopFlight();
    if (m_tour)
    {
        m_tour->stop();
    }
}

void AppController::tickDemo()
{
    m_demo.tick();
}

void AppController::takeSnapshot()
{
    if (m_snapshot && (m_demo.playing() || tourRunning()))
    {
        return;  // keep the view from before the demo that is running
    }
    m_snapshot = Snapshot{.settings = m_settings, .showOrbit = m_canvas.showOrbit()};
}

void AppController::restoreSnapshot()
{
    if (!m_snapshot)
    {
        return;
    }
    const Snapshot snapshot = *m_snapshot;
    m_snapshot.reset();
    stopDemos();
    applySettings(snapshot.settings);
    setShowOrbit(snapshot.showOrbit);
}

void AppController::showExportDialog() const
{
    call(m_shell.showExportDialog);
}

void AppController::closeExportDialog() const
{
    call(m_shell.closeExportDialog);
}

void AppController::showHelpPage(std::string_view page) const
{
    call(m_shell.showHelpPage, page);
}

bool AppController::handleGlobalKey(GlobalKey key)
{
    const bool demoRunning = m_demo.playing() || tourRunning();
    if (key == GlobalKey::ESCAPE && demoRunning)
    {
        stopDemos();
        return true;
    }
    if (m_demo.playing())
    {
        stopFlight();  // any key ends a flight; the key then does its usual job
    }
    return false;
}

}  // namespace mandelbrotter::app
