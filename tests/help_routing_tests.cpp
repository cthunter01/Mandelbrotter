#include "Mandelbrotter/app/help_routing.h"

#include <optional>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/app/panel_model.h"

namespace
{

using mandelbrotter::app::HelpContext;
using mandelbrotter::app::HelpLinkKind;
using mandelbrotter::app::PanelSection;
using Area = HelpContext::Area;

HelpContext focus(Area area, std::optional<PanelSection> section = std::nullopt,
                  bool juliaControl = false)
{
    return {.area = area, .section = section, .juliaControl = juliaControl};
}

struct Route
{
    HelpContext      context;
    std::string_view page;
};

TEST(HelpRouting, FocusPicksThePage)
{
    const std::vector<Route> routes{
        {focus(Area::NONE), "index.html"},
        {focus(Area::CANVAS), "navigating.html"},
        {focus(Area::EXPORT_DIALOG), "exporting.html"},
        {focus(Area::PANEL), "index.html"},  // the panel itself, outside every box
        {focus(Area::PANEL, PanelSection::FRACTAL), "fractals.html"},
        {focus(Area::PANEL, PanelSection::FRACTAL, true), "julia.html"},
        {focus(Area::PANEL, PanelSection::ITERATIONS), "iterations.html"},
        {focus(Area::PANEL, PanelSection::COLORING), "coloring.html"},
        {focus(Area::PANEL, PanelSection::OVERLAY), "orbit.html"},
        {focus(Area::PANEL, PanelSection::BOOKMARKS), "bookmarks.html"},
    };
    for (const Route& route : routes)
    {
        EXPECT_EQ(mandelbrotter::app::helpPageFor(route.context), route.page) << route.page;
    }
}

TEST(HelpRouting, TheJuliaFlagOnlyMattersInTheFractalBox)
{
    EXPECT_EQ(mandelbrotter::app::helpPageFor(focus(Area::PANEL, PanelSection::COLORING, true)),
              "coloring.html");
    EXPECT_EQ(mandelbrotter::app::helpPageFor(focus(Area::CANVAS, std::nullopt, true)),
              "navigating.html");
}

TEST(HelpRouting, LinksAreActionsExternalOrPages)
{
    using mandelbrotter::app::classifyHelpLink;
    EXPECT_EQ(classifyHelpLink("mandelbrotter:tour"), HelpLinkKind::ACTION);
    EXPECT_EQ(classifyHelpLink("mandelbrotter:view --zoom 5000"), HelpLinkKind::ACTION);
    EXPECT_EQ(classifyHelpLink("mandelbrotter:"), HelpLinkKind::ACTION);  // malformed, still ours
    EXPECT_EQ(classifyHelpLink("https://en.wikipedia.org/wiki/Mandelbrot_set"),
              HelpLinkKind::EXTERNAL);
    EXPECT_EQ(classifyHelpLink("http://example.com"), HelpLinkKind::EXTERNAL);
    EXPECT_EQ(classifyHelpLink("page.html"), HelpLinkKind::PAGE);
    EXPECT_EQ(classifyHelpLink("julia.html#seeds"), HelpLinkKind::PAGE);
    EXPECT_EQ(classifyHelpLink("ftp://example.com"), HelpLinkKind::PAGE);
}

}  // namespace
