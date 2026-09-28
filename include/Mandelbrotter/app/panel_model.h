#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "Mandelbrotter/Palette.h"
#include "Mandelbrotter/RenderSettings.h"
#include "Mandelbrotter/fractal.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::app
{

/// The side panel's boxes, top to bottom.
enum class PanelSection : std::uint8_t
{
    FRACTAL,
    ITERATIONS,
    COLORING,
    OVERLAY,
    BOOKMARKS,
};
inline constexpr std::size_t kPanelSectionCount = 5;

/// The Offset slider's resolution: one cycle in this many steps.
inline constexpr int kOffsetSliderSteps = 1000;
/// The Julia preview: a fixed, small iteration limit, and at most one render per throttle period.
inline constexpr int  kPreviewIterations = 128;
inline constexpr auto kPreviewThrottle   = std::chrono::milliseconds(50);

/// The slider position of a palette offset: its phase (the fraction of a cycle) in slider steps.
[[nodiscard]] int offsetToSlider(double offset) noexcept;
/// The palette offset a slider position stands for.
[[nodiscard]] double sliderToOffset(int position) noexcept;

/// The seed typed into the two seed fields (spaces around the numbers are fine); nullopt when
/// either is not a finite number.
[[nodiscard]] std::optional<Complex> parseSeed(std::string_view re, std::string_view im);

/// What committing the two seed fields (Enter, or the focus leaving one) does.
struct SeedEdit
{
    enum class Kind : std::uint8_t
    {
        APPLY,      ///< a new valid seed: use it
        UNCHANGED,  ///< the fields hold the current seed
        RESTORE,    ///< a field does not parse: show the current seed again
    };
    Kind    kind{Kind::UNCHANGED};
    Complex seed;  ///< APPLY: the new seed; RESTORE: the value to show again
};
/// The seed fields were committed: apply a new valid seed, ignore an unchanged one, restore the
/// last valid value (`current`) when either field is not a number.
[[nodiscard]] SeedEdit commitSeedFields(std::string_view re, std::string_view im, Complex current);

/// The seed the Julia preview shows: the hovered point, or the current seed in Julia mode.
[[nodiscard]] std::optional<Complex> previewSeedFor(const FractalSpec&     spec,
                                                    std::optional<Complex> hovered) noexcept;

/// The positions in the Family and Palette lists (allFamilies(), paletteNames()); nullopt for a
/// value the list does not hold.
[[nodiscard]] std::optional<std::size_t> familyIndex(FractalFamily family) noexcept;
[[nodiscard]] std::optional<std::size_t> paletteIndex(std::string_view name) noexcept;

/// "Effective limit: 1234".
[[nodiscard]] std::string effectiveLimitText(int iterations);

/// What the Julia preview renders for `seed`: the family and exponent of `spec` in Julia mode,
/// its default view, kPreviewIterations (no auto) and `coloring`.
[[nodiscard]] RenderSettings previewSettings(const FractalSpec& spec, Complex seed,
                                             const ColoringSettings& coloring);

}  // namespace mandelbrotter::app
