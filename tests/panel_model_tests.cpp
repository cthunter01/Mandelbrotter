#include "Mandelbrotter/app/panel_model.h"

#include <cstddef>
#include <optional>

#include <gtest/gtest.h>

#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace
{

using mandelbrotter::Complex;
using mandelbrotter::FractalFamily;
using mandelbrotter::FractalSpec;
namespace app = mandelbrotter::app;

TEST(PanelModel, SliderShowsThePhaseOfTheOffset)
{
    EXPECT_EQ(app::offsetToSlider(0.0), 0);
    EXPECT_EQ(app::offsetToSlider(0.25), 250);
    EXPECT_EQ(app::offsetToSlider(1.25), 250);
    EXPECT_EQ(app::offsetToSlider(1.999), 999);
    EXPECT_EQ(app::offsetToSlider(-0.25), 750);
    EXPECT_EQ(app::offsetToSlider(-3.0), 0);
    EXPECT_EQ(app::offsetToSlider(0.9999), app::kOffsetSliderSteps);  // rounds up to a full cycle
}

TEST(PanelModel, SliderPositionsRoundTrip)
{
    for (const int position : {0, 1, 250, 500, 999})
    {
        EXPECT_EQ(app::offsetToSlider(app::sliderToOffset(position)), position);
    }
    EXPECT_DOUBLE_EQ(app::sliderToOffset(250), 0.25);
    EXPECT_DOUBLE_EQ(app::sliderToOffset(app::kOffsetSliderSteps), 1.0);
}

TEST(PanelModel, SeedFieldsParseWithSpacesAround)
{
    EXPECT_EQ(app::parseSeed("-0.8", "0.156"), (Complex{-0.8, 0.156}));
    EXPECT_EQ(app::parseSeed("  0.285 ", " 0.01"), (Complex{0.285, 0.01}));
    EXPECT_EQ(app::parseSeed("1e-3", "-2"), (Complex{1e-3, -2.0}));
}

TEST(PanelModel, SeedFieldsFailWhenEitherIsBad)
{
    EXPECT_EQ(app::parseSeed("", "0"), std::nullopt);
    EXPECT_EQ(app::parseSeed("0", "abc"), std::nullopt);
    EXPECT_EQ(app::parseSeed("0.5x", "0"), std::nullopt);
    EXPECT_EQ(app::parseSeed("0", "inf"), std::nullopt);
    EXPECT_EQ(app::parseSeed("0\t", "0"), std::nullopt);  // only spaces are trimmed
}

TEST(PanelModel, PreviewFollowsTheMouseOutsideJuliaMode)
{
    FractalSpec spec;
    spec.seed = {0.1, 0.2};
    EXPECT_EQ(app::previewSeedFor(spec, Complex{0.3, 0.4}), (Complex{0.3, 0.4}));
    EXPECT_EQ(app::previewSeedFor(spec, std::nullopt), std::nullopt);

    spec.julia = true;
    EXPECT_EQ(app::previewSeedFor(spec, Complex{0.3, 0.4}), (Complex{0.1, 0.2}));
    EXPECT_EQ(app::previewSeedFor(spec, std::nullopt), (Complex{0.1, 0.2}));
}

TEST(PanelModel, EveryFamilyAndPaletteHasItsListPosition)
{
    const auto families = mandelbrotter::allFamilies();
    for (std::size_t i = 0; i < families.size(); ++i)
    {
        EXPECT_EQ(app::familyIndex(families[i]), i);
    }

    const auto names = mandelbrotter::paletteNames();
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        EXPECT_EQ(app::paletteIndex(names[i]), i);
    }
    EXPECT_EQ(app::paletteIndex("no-such-palette"), std::nullopt);
    EXPECT_EQ(app::paletteIndex(""), std::nullopt);
}

TEST(PanelModel, EffectiveLimitText)
{
    EXPECT_EQ(app::effectiveLimitText(256), "Effective limit: 256");
    EXPECT_EQ(app::effectiveLimitText(1000000), "Effective limit: 1000000");
}

TEST(PanelModel, PreviewSettingsAreASmallJuliaSetOfTheFamily)
{
    FractalSpec spec;
    spec.family   = FractalFamily::TRICORN;
    spec.exponent = 3;
    spec.seed     = {9.0, 9.0};  // ignored
    const mandelbrotter::ColoringSettings coloring{
        .palette = "ocean", .density = 12.0, .offset = 0.5};

    const mandelbrotter::RenderSettings settings =
        app::previewSettings(spec, {-0.8, 0.156}, coloring);
    EXPECT_EQ(settings.fractal.family, FractalFamily::TRICORN);
    EXPECT_EQ(settings.fractal.exponent, 3);
    EXPECT_TRUE(settings.fractal.julia);
    EXPECT_EQ(settings.fractal.seed, (Complex{-0.8, 0.156}));
    EXPECT_EQ(settings.view, mandelbrotter::defaultView(settings.fractal));
    EXPECT_EQ(settings.maxIterations, app::kPreviewIterations);
    EXPECT_EQ(settings.maxIterations, 128);
    EXPECT_FALSE(settings.autoIterations);
    EXPECT_EQ(settings.coloring, coloring);
}

}  // namespace
