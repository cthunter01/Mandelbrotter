#include "gui/HelpController.h"

#include <cstddef>
#include <string>
#include <string_view>

#include <wx/fs_mem.h>
#include <wx/html/helpfrm.h>
#include <wx/html/helpwnd.h>
#include <wx/html/htmlwin.h>
#include <wx/treectrl.h>
#include <wx/utils.h>

#include "Mandelbrotter/help_action.h"
#include "gui/app_icon.h"
#include "gui/help_book.h"
#include "gui/wx_util.h"

namespace mandelbrotter::gui
{

namespace
{

// wxHF_DEFAULT_STYLE minus wxHF_BOOKMARKS: help-window bookmarks would be confused with the
// application's.
constexpr int kHelpStyle =
    wxHF_TOOLBAR | wxHF_FLAT_TOOLBAR | wxHF_CONTENTS | wxHF_INDEX | wxHF_SEARCH | wxHF_PRINT;
constexpr const char* kBookFile = "help.zip";

}  // namespace

std::string_view helpBookZip()
{
    static const std::string kZip = [] {
        std::size_t total = 0;
        for (const std::string_view chunk : helpBookChunks())
        {
            total += chunk.size();
        }
        std::string zip;
        zip.reserve(total);
        for (const std::string_view chunk : helpBookChunks())
        {
            zip.append(chunk);
        }
        return zip;
    }();
    return kZip;
}

HelpController::HelpController() : wxHtmlHelpController(kHelpStyle, nullptr)
{
    SetTitleFormat("Mandelbrotter Help: %s");
}

bool HelpController::ensureLoaded()
{
    if (m_loaded || m_loadFailed)
    {
        return m_loaded;
    }
    // The book is served from memory: wxMemoryFSHandler and wxArchiveFSHandler are registered by
    // the application (MandelbrotterApp::OnInit), so "memory:help.zip#zip:page.html" resolves.
    const std::string_view zip = helpBookZip();
    wxMemoryFSHandler::AddFileWithMimeType(kBookFile, zip.data(), zip.size(), "application/zip");
    m_loaded = AddBook(wxString("memory:") + kBookFile);
    if (!m_loaded)
    {
        m_loadFailed = true;
        if (onError)
        {
            onError("The help book built into this program could not be read.");
        }
    }
    return m_loaded;
}

void HelpController::showContents()
{
    if (ensureLoaded())
    {
        DisplayContents();
    }
}

void HelpController::showPage(std::string_view page)
{
    if (ensureLoaded() && !Display(toWx(page)))
    {
        DisplayContents();
    }
}

wxHtmlHelpFrame* HelpController::CreateHelpFrame(wxHtmlHelpData* data)
{
    wxHtmlHelpFrame* frame = wxHtmlHelpController::CreateHelpFrame(data);
    applyAppIcon(*frame);
    frame->Bind(wxEVT_HTML_LINK_CLICKED, &HelpController::onLinkClicked, this);
    // The tree starts with the book collapsed to its title; open it once the frame is up.
    frame->CallAfter([frame] {
        if (const wxHtmlHelpWindow* window = frame->GetHelpWindow(); window != nullptr)
        {
            if (wxTreeCtrl* tree = window->GetTreeCtrl(); tree != nullptr)
            {
                tree->ExpandAll();
            }
        }
    });
    return frame;
}

// Not const: wxEvtHandler::Bind takes a non-const member function.
// NOLINTNEXTLINE(readability-make-member-function-const)
void HelpController::onLinkClicked(wxHtmlLinkEvent& event)
{
    const wxString&   href = event.GetLinkInfo().GetHref();
    const std::string url  = fromWx(href);
    if (isHelpActionUrl(url))
    {
        // Handled here (no Skip): the help window stays on its page.
        const auto action = parseHelpAction(url);
        if (action)
        {
            if (onAction)
            {
                onAction(*action);
            }
        }
        else if (onError)
        {
            onError(action.error());
        }
        return;
    }
    if (url.starts_with("http://") || url.starts_with("https://"))
    {
        wxLaunchDefaultBrowser(href);
        return;
    }
    event.Skip();  // a page of the book
}

}  // namespace mandelbrotter::gui
