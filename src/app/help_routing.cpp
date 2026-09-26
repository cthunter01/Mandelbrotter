#include "Mandelbrotter/app/help_routing.h"

#include <string_view>

#include "Mandelbrotter/app/panel_model.h"
#include "Mandelbrotter/help_action.h"

namespace mandelbrotter::app
{

namespace
{

constexpr std::string_view kIndexPage = "index.html";

std::string_view panelPageFor(PanelSection section, bool juliaControl) noexcept
{
    switch (section)
    {
        case PanelSection::FRACTAL:
            return juliaControl ? "julia.html" : "fractals.html";
        case PanelSection::ITERATIONS:
            return "iterations.html";
        case PanelSection::COLOURING:
            return "colouring.html";
        case PanelSection::OVERLAY:
            return "orbit.html";
        case PanelSection::BOOKMARKS:
            return "bookmarks.html";
    }
    return kIndexPage;
}

}  // namespace

std::string_view helpPageFor(const HelpContext& context) noexcept
{
    switch (context.area)
    {
        case HelpContext::Area::NONE:
            break;
        case HelpContext::Area::CANVAS:
            return "navigating.html";
        case HelpContext::Area::PANEL:
            if (context.section)
            {
                return panelPageFor(*context.section, context.juliaControl);
            }
            break;
        case HelpContext::Area::EXPORT_DIALOG:
            return "exporting.html";
    }
    return kIndexPage;
}

HelpLinkKind classifyHelpLink(std::string_view url) noexcept
{
    if (isHelpActionUrl(url))
    {
        return HelpLinkKind::ACTION;
    }
    if (url.starts_with("http://") || url.starts_with("https://"))
    {
        return HelpLinkKind::EXTERNAL;
    }
    return HelpLinkKind::PAGE;
}

}  // namespace mandelbrotter::app
