#include "Mandelbrotter/app/ui_text.h"

#include <cstddef>
#include <filesystem>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/panel_model.h"
#include "Mandelbrotter/exporter.h"

namespace
{

namespace app = mandelbrotter::app;

TEST(UiText, EveryTextIsAsciiAndNonEmpty)
{
    std::vector<std::string_view> texts{
        app::kWindowTitle,
        app::kAboutName,
        app::kAboutDescription,
        app::kPngFilter.description,
        app::kPngFilter.pattern,
        app::kViewFilter.description,
        app::kViewFilter.pattern,
        app::kAllFilesFilter.description,
        app::kAllFilesFilter.pattern,
        app::kSaveImageTitle,
        app::kSaveImageDefaultName,
        app::kExportViewTitle,
        app::kExportViewDefaultName,
        app::kImportViewTitle,
        app::kAddBookmarkTitle,
        app::kAddBookmarkPrompt,
        app::kCopyImageTitle,
        app::kClipboardBusy,
        app::kHelpErrorTitle,
        app::kHelpBookUnreadable,
        app::kSaveFailedTitle,
        app::kSavingTitle,
        app::kCanceling,
        app::kExportDialogTitle,
        app::kWidthLabel,
        app::kHeightLabel,
        app::kAntiAliasingLabel,
        app::kExportNote,
        app::kFamilyLabel,
        app::kExponentLabel,
        app::kJuliaLabel,
        app::kSeedPrefix,
        app::kSeedPlus,
        app::kSeedSuffix,
        app::kPickSeedLabel,
        app::kPickSeedTip,
        app::kPreviewLabel,
        app::kMaximumLabel,
        app::kAutoIterationsLabel,
        app::kPaletteLabel,
        app::kDensityLabel,
        app::kDensityTip,
        app::kOffsetLabel,
        app::kOffsetTip,
        app::kShowOrbitLabel,
        app::kAddBookmarkButton,
        app::kAddBookmarkTip,
        app::kLoadBookmarkButton,
        app::kDeleteBookmarkButton,
        app::kLearnMore,
        app::kClose,
        app::kBack,
        app::kNext,
        app::kFinish,
        app::kHelpTitlePrefix,
    };
    for (const app::SupersampleChoice& choice : app::kSupersampleChoices)
    {
        texts.push_back(choice.label);
    }
    for (std::size_t i = 0; i < app::kPanelSectionCount; ++i)
    {
        texts.push_back(app::sectionTitle(static_cast<app::PanelSection>(i)));
    }
    for (const std::string_view text : texts)
    {
        EXPECT_FALSE(text.empty());
        for (const char c : text)
        {
            EXPECT_LT(static_cast<unsigned char>(c), 0x80) << text;
        }
    }
}

TEST(UiText, FormattedTexts)
{
    EXPECT_EQ(app::stepText(0, 11), "Step 1 of 11");
    EXPECT_EQ(app::stepText(10, 11), "Step 11 of 11");
    EXPECT_EQ(app::renderingText({1920, 1080}), "Rendering 1920x1080...");
    EXPECT_EQ(
        app::saveFailedText(std::filesystem::path("out") / "a.png", "disk full"),
        "Could not save " + (std::filesystem::path("out") / "a.png").string() + ":\ndisk full");
}

TEST(UiText, SectionTitles)
{
    EXPECT_EQ(app::sectionTitle(app::PanelSection::FRACTAL), "Fractal");
    EXPECT_EQ(app::sectionTitle(app::PanelSection::ITERATIONS), "Iterations");
    EXPECT_EQ(app::sectionTitle(app::PanelSection::COLORING), "Coloring");
    EXPECT_EQ(app::sectionTitle(app::PanelSection::OVERLAY), "Overlay");
    EXPECT_EQ(app::sectionTitle(app::PanelSection::BOOKMARKS), "Bookmarks");
}

TEST(UiText, TheSupersampleChoicesGrowUpToTheMaximum)
{
    int previous = 0;
    for (const app::SupersampleChoice& choice : app::kSupersampleChoices)
    {
        EXPECT_GT(choice.factor, previous);
        previous = choice.factor;
    }
    EXPECT_EQ(app::kSupersampleChoices.front().factor, 1);
    EXPECT_EQ(previous, mandelbrotter::kMaxSupersample);
}

TEST(UiText, OneStatusWidthPerField)
{
    EXPECT_EQ(app::kStatusStretch.size(), static_cast<std::size_t>(app::kStatusFieldCount));
    for (const int stretch : app::kStatusStretch)
    {
        EXPECT_GT(stretch, 0);
    }
}

}  // namespace
