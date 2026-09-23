#pragma once

#include <functional>
#include <string>
#include <string_view>

#include <wx/html/helpctrl.h>

#include "Mandelbrotter/help_action.h"

namespace mandelbrotter::gui
{

/// The help window: wx's HTML help viewer (contents tree, index, full-text search, back/forward)
/// over the book embedded in the executable (help_book.h). Links of the form "mandelbrotter:..."
/// are not pages but actions on the main window; they are reported through onAction.
class HelpController : public wxHtmlHelpController
{
public:
    HelpController();

    void showContents();
    /// Opens a page of the book by file name ("navigating.html"); an unknown page opens the
    /// contents.
    void showPage(std::string_view page);

    std::function<void(const HelpAction&)>  onAction;
    std::function<void(const std::string&)> onError;  ///< a malformed action link, a missing book

protected:
    /// The frame is created on first use and again after the user closes it, so the link handler
    /// is attached here.
    wxHtmlHelpFrame* CreateHelpFrame(wxHtmlHelpData* data) override;

private:
    bool ensureLoaded();
    void onLinkClicked(wxHtmlLinkEvent& event);

    bool m_loaded{false};
    bool m_loadFailed{false};
};

}  // namespace mandelbrotter::gui
