#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include "Mandelbrotter/app/panel_model.h"
#include "Mandelbrotter/geometry.h"

/// The texts and sizes every toolkit's window shows, in one place so the front ends and the help
/// book that describes them cannot drift apart. Texts are ASCII: wx turns a non-ASCII narrow
/// literal into an empty string under the C locale. Sizes are logical (device-independent) pixels.
namespace mandelbrotter::app
{

// ---- the main window
inline constexpr std::string_view kWindowTitle = "Mandelbrotter";
/// The size of the window's content (without the menu and status bars) when it opens.
inline constexpr PixelSize kWindowSize{1280, 800};
/// The status bar fields' relative widths, one per StatusField.
inline constexpr std::array<int, 5> kStatusStretch{3, 3, 1, 1, 2};
inline constexpr std::string_view   kAboutName = "Mandelbrotter";
inline constexpr std::string_view   kAboutDescription =
    "Interactive Mandelbrot-family fractal explorer.\n\n"
    "The user guide is under Help > Contents; F1 explains the focused control.";

// ---- files
/// A file dialog's filter: {"PNG images", "*.png"}.
struct FileFilter
{
    std::string_view description;
    std::string_view pattern;
};
inline constexpr FileFilter       kPngFilter{"PNG images", "*.png"};
inline constexpr FileFilter       kViewFilter{"Mandelbrotter views", "*.json"};
inline constexpr FileFilter       kAllFilesFilter{"All files", "*"};
inline constexpr std::string_view kSaveImageTitle        = "Save image as PNG";
inline constexpr std::string_view kSaveImageDefaultName  = "mandelbrotter.png";
inline constexpr std::string_view kExportViewTitle       = "Export view";
inline constexpr std::string_view kExportViewDefaultName = "view.json";
inline constexpr std::string_view kImportViewTitle       = "Import view";

// ---- dialogs and errors
inline constexpr std::string_view kAddBookmarkTitle  = "Add bookmark";
inline constexpr std::string_view kAddBookmarkPrompt = "Name for this view:";
inline constexpr std::string_view kCopyImageTitle    = "Copy image";
inline constexpr std::string_view kClipboardBusy     = "The clipboard is busy.";
inline constexpr std::string_view kHelpErrorTitle    = "Help";
inline constexpr std::string_view kHelpBookUnreadable =
    "The help book built into this program could not be read.";
inline constexpr std::string_view kSaveFailedTitle = "Save image failed";
inline constexpr std::string_view kSavingTitle     = "Saving image";
inline constexpr std::string_view kCanceling       = "Canceling...";
/// "Could not save <path>:\n<error>".
[[nodiscard]] std::string saveFailedText(const std::filesystem::path& path, std::string_view error);
/// The export's progress text: "Rendering 1920x1080...".
[[nodiscard]] std::string renderingText(PixelSize size);

// ---- the Save image as PNG dialog
inline constexpr std::string_view kExportDialogTitle = "Save image as PNG";
inline constexpr std::string_view kWidthLabel        = "Width (px)";
inline constexpr std::string_view kHeightLabel       = "Height (px)";
inline constexpr std::string_view kAntiAliasingLabel = "Anti-aliasing";
inline constexpr std::string_view kExportNote =
    "The view keeps its center and zoom; the shorter side shows the same extent.";
/// One entry of the Anti-aliasing list: samples per pixel along each axis.
struct SupersampleChoice
{
    int              factor;
    std::string_view label;
};
inline constexpr std::array<SupersampleChoice, 3> kSupersampleChoices{
    {
        {1, "1x (none)"},
        {2, "2x2 samples per pixel"},
        {4, "4x4 samples per pixel"},
    },
};

// ---- the side panel
/// The title of a section's box: "Fractal", "Iterations", ...
[[nodiscard]] std::string_view sectionTitle(PanelSection section) noexcept;

inline constexpr std::string_view kFamilyLabel   = "Family";
inline constexpr std::string_view kExponentLabel = "Exponent n";
inline constexpr std::string_view kJuliaLabel    = "Julia set (z0 = pixel, c = seed)";
inline constexpr std::string_view kSeedPrefix    = "c =";  ///< the seed row: "c = [re] + [im] i"
inline constexpr std::string_view kSeedPlus      = "+";
inline constexpr std::string_view kSeedSuffix    = "i";
inline constexpr std::string_view kPickSeedLabel = "Pick seed from canvas";
inline constexpr std::string_view kPickSeedTip =
    "Click a point of the set to use it as the Julia constant c";
inline constexpr std::string_view kPreviewLabel        = "Julia preview (follows the mouse)";
inline constexpr std::string_view kMaximumLabel        = "Maximum";
inline constexpr std::string_view kAutoIterationsLabel = "Auto: grow with zoom";
inline constexpr std::string_view kPaletteLabel        = "Palette";
inline constexpr std::string_view kDensityLabel        = "Density";
inline constexpr std::string_view kDensityTip          = "Iterations per palette cycle";
inline constexpr std::string_view kOffsetLabel         = "Offset";
inline constexpr std::string_view kOffsetTip           = "Palette phase shift";
inline constexpr std::string_view kShowOrbitLabel      = "Show orbit of the point under the cursor";
inline constexpr std::string_view kAddBookmarkButton   = "Add...";
inline constexpr std::string_view kAddBookmarkTip      = "Bookmark the current view";
inline constexpr std::string_view kLoadBookmarkButton  = "Load";
inline constexpr std::string_view kDeleteBookmarkButton = "Delete";

/// The Density spin box: its step and decimals (its range is kMinDensity..kMaxDensity).
inline constexpr double kDensityStep     = 1.0;
inline constexpr int    kDensityDigits   = 1;
inline constexpr int    kSectionBorder   = 6;   ///< around each section's box
inline constexpr int    kRowGap          = 3;   ///< around each row inside a box
inline constexpr int    kLabelGap        = 6;   ///< between a label and its control
inline constexpr int    kSeedGap         = 4;   ///< around the seed row's "+" and "i"
inline constexpr int    kPanelScrollStep = 10;  ///< the panel scrolls vertically in these steps
inline constexpr int    kPanelWidthSlack = 24;  ///< beside the widest row: the scroll bar
inline constexpr int    kPanelMinHeight  = 200;
inline constexpr int    kBookmarkListMinHeight = 120;
inline constexpr int    kScrollToMargin        = 6;  ///< above a section scrolled into view

// ---- the canvas, the Julia preview and their overlays
inline constexpr PixelSize kCanvasMinSize{200, 150};
inline constexpr PixelSize kPreviewMinSize{200, 150};
struct Rgba
{
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
    std::uint8_t alpha;
};
inline constexpr Rgba kOrbitColor{255, 255, 255, 200};
inline constexpr int  kOrbitWidth = 1;
/// The circle at the orbit's first point.
inline constexpr Rgba kOrbitStartColor{255, 80, 80, 255};
inline constexpr int  kOrbitStartWidth    = 2;
inline constexpr int  kOrbitStartRadius   = 4;
inline constexpr int  kRubberBandWidth    = 1;  ///< black solid, then white short dashes
inline constexpr int  kHighlightRingWidth = 3;  ///< the tour's ring around the canvas
inline constexpr int  kSectionRingWidth   = 3;  ///< the tour's ring around a panel section
inline constexpr int  kSectionRingRadius  = 4;
inline constexpr int  kSectionRingInflate = 3;  ///< how far outside the section's box

// ---- the tour's card
inline constexpr std::string_view kLearnMore = "Learn more";
inline constexpr std::string_view kClose     = "Close";
inline constexpr std::string_view kBack      = "Back";
inline constexpr std::string_view kNext      = "Next";
inline constexpr std::string_view kFinish    = "Finish";
/// "Step 1 of 11" for `index` 0 of `count` 11.
[[nodiscard]] std::string stepText(std::size_t index, std::size_t count);
inline constexpr int      kCardTextWidth = 300;  ///< the text wraps at this width
inline constexpr int      kCardPadding   = 12;
inline constexpr int      kCardGap       = 12;  ///< between the card and its anchor
inline constexpr int      kCardBorder    = 2;
inline constexpr int      kCardButtonGap = 4;

// ---- the help window
/// Followed by the page's title.
inline constexpr std::string_view kHelpTitlePrefix = "Mandelbrotter Help: ";

}  // namespace mandelbrotter::app
