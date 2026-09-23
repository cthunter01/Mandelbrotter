#pragma once

#include <cstddef>
#include <optional>

#include <wx/frame.h>
#include <wx/menu.h>

#include "Mandelbrotter/BigComplex.h"
#include "Mandelbrotter/RenderSettings.h"
#include "gui/BookmarkStore.h"
#include "gui/FractalCanvas.h"

namespace mandelbrotter::gui
{

class SidePanel;

class MainFrame : public wxFrame
{
public:
    explicit MainFrame(RenderSettings initial);

private:
    void buildMenus();
    void wireCanvas();
    void wirePanel();

    /// Makes `settings` the current model everywhere (canvas, panel, status bar).
    void applySettings(const RenderSettings& settings);
    void updateStatusBar();
    void showPointer(const std::optional<BigComplex>& pointer);
    void showRenderStatus(const FractalCanvas::RenderStatus& status);

    void saveImage();
    void copyImage();
    void exportView();
    void importView();
    void addBookmark();
    void loadBookmark(std::size_t index);
    void deleteBookmark(std::size_t index);
    void refreshBookmarks();
    void reportError(const std::string& title, const std::string& message);

    RenderSettings m_settings;
    BookmarkStore  m_bookmarks;
    FractalCanvas* m_canvas{nullptr};
    SidePanel*     m_panel{nullptr};
    wxMenuItem*    m_showPanelItem{nullptr};
    wxMenuItem*    m_showOrbitItem{nullptr};
};

}  // namespace mandelbrotter::gui
