#include "Mandelbrotter/app/ui_text.h"

#include <cstddef>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/panel_model.h"
#include "Mandelbrotter/exporter.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::app
{

static_assert(kStatusStretch.size() == kStatusFieldCount);
static_assert(kSupersampleChoices.back().factor == kMaxSupersample);

std::string saveFailedText(const std::filesystem::path& path, std::string_view error)
{
    return std::format("Could not save {}:\n{}", path.string(), error);
}

std::string renderingText(PixelSize size)
{
    return std::format("Rendering {}x{}...", size.width, size.height);
}

std::string_view sectionTitle(PanelSection section) noexcept
{
    switch (section)
    {
        case PanelSection::FRACTAL:
            return "Fractal";
        case PanelSection::ITERATIONS:
            return "Iterations";
        case PanelSection::COLORING:
            return "Coloring";
        case PanelSection::OVERLAY:
            return "Overlay";
        case PanelSection::BOOKMARKS:
            return "Bookmarks";
    }
    return {};
}

std::string stepText(std::size_t index, std::size_t count)
{
    return std::format("Step {} of {}", index + 1, count);
}

}  // namespace mandelbrotter::app
