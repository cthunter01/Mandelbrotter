#include "qt/ExportDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QGuiApplication>
#include <QImage>
#include <QKeySequence>
#include <QLineEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QSize>
#include <QSpinBox>
#include <QTest>
#include <QTimer>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Mandelbrotter/app/AppController.h"
#include "Mandelbrotter/app/help_routing.h"
#include "Mandelbrotter/app/scenes.h"
#include "Mandelbrotter/app/ui_text.h"
#include "Mandelbrotter/exporter.h"
#include "QtHarness.h"
#include "qt/FractalCanvas.h"
#include "qt/MainWindow.h"
#include "qt/SidePanel.h"
#include "qt/qt_util.h"

namespace
{

using mandelbrotter::PixelSize;
using mandelbrotter::app::Command;
using mandelbrotter::app::StatusField;
using mandelbrotter::test::pumpUntil;
using mandelbrotter::test::QtHarness;
namespace app = mandelbrotter::app;
namespace qt  = mandelbrotter::qt;

/// A main window whose dialogs are answered by the test.
struct DialogHarness : QtHarness
{
    DialogHarness()
    {
        window.dialogSeams.chooseFile = [this](qt::MainWindow::FileChoice choice) {
            choices.push_back(choice);
            return file;
        };
        window.dialogSeams.showError = [this](std::string_view title, std::string_view message) {
            errors.emplace_back(std::string(title) + ": " + std::string(message));
        };
        EXPECT_TRUE(pumpUntil([this] { return renderDone(); }));
    }

    /// Opens the dialog with the given options and presses OK.
    void save(const mandelbrotter::ExportOptions& options)
    {
        window.showExportDialog();
        qt::ExportDialog* dialog = window.exportDialog();
        ASSERT_NE(dialog, nullptr);
        dialog->widthField().setValue(options.size.width);
        dialog->heightField().setValue(options.size.height);
        for (std::size_t i = 0; i < app::kSupersampleChoices.size(); ++i)
        {
            if (app::kSupersampleChoices.at(i).factor == options.supersample)
            {
                dialog->supersampleField().setCurrentIndex(static_cast<int>(i));
            }
        }
        dialog->accept();
    }

    std::optional<std::filesystem::path>    file;
    std::vector<qt::MainWindow::FileChoice> choices;
    std::vector<std::string>                errors;
};

bool isPng(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::string   signature(8, '\0');
    in.read(signature.data(), 8);
    return in && signature == "\x89PNG\r\n\x1a\n";
}

TEST(ExportDialog, StartsAtTheCanvasPictureSize)
{
    DialogHarness h;
    h.window.showExportDialog();
    const qt::ExportDialog* dialog = h.window.exportDialog();
    ASSERT_NE(dialog, nullptr);
    const PixelSize picture = h.window.canvas().controller().image().size();
    EXPECT_EQ(dialog->widthField().value(), picture.width);
    EXPECT_EQ(dialog->heightField().value(), picture.height);
    EXPECT_EQ(dialog->widthField().maximum(), mandelbrotter::kMaxExportDimension);
    EXPECT_EQ(dialog->windowTitle(), qt::toQt(app::kExportDialogTitle));

    const qt::ExportDialog huge(nullptr, {99999, 0});
    EXPECT_EQ(huge.widthField().value(), mandelbrotter::kMaxExportDimension);
    EXPECT_EQ(huge.heightField().value(), 1);
}

TEST(ExportDialog, TheAntiAliasingChoicesAreOneTwoAndFour)
{
    const qt::ExportDialog dialog(nullptr, {640, 400});
    ASSERT_EQ(dialog.supersampleField().count(), 3);
    for (int i = 0; i < 3; ++i)
    {
        dialog.supersampleField().setCurrentIndex(i);
        EXPECT_EQ(dialog.options().supersample,
                  app::kSupersampleChoices.at(static_cast<std::size_t>(i)).factor);
    }
    EXPECT_EQ(dialog.options().size, (PixelSize{640, 400}));
}

TEST(ExportDialog, OkWritesThePngAndReportsIt)
{
    DialogHarness h;
    h.file = h.dir / "picture.png";
    h.save({.size = {120, 80}, .supersample = 2});
    EXPECT_EQ(h.window.exportDialog(), nullptr);  // closed before the file dialog
    EXPECT_EQ(h.choices, std::vector{qt::MainWindow::FileChoice::SAVE_IMAGE});
    ASSERT_TRUE(isPng(*h.file));
    const QImage written(qt::fromPath(*h.file));
    EXPECT_EQ(written.size(), QSize(120, 80));
    EXPECT_EQ(h.status(StatusField::RENDER), "Saved picture.png");
    EXPECT_TRUE(h.errors.empty());
}

TEST(ExportDialog, CancelingTheFileDialogSavesNothing)
{
    DialogHarness h;
    h.save({.size = {60, 40}, .supersample = 1});
    EXPECT_EQ(h.choices.size(), 1U);
    EXPECT_FALSE(h.status(StatusField::RENDER).starts_with("Saved"));
}

TEST(ExportDialog, CancelingTheExportLeavesNoFileAndNoError)
{
    DialogHarness h;
    h.window.app().applySettings(app::deepSeahorse(app::kScreenshotDeepZoom));
    h.file = h.dir / "big.png";
    // Big and deep enough to be canceled while it runs; the progress dialog is modal, so the
    // cancel comes from a timer.
    bool canceled = false;
    QTimer::singleShot(300, [&canceled] {
        if (auto* progress = qobject_cast<QProgressDialog*>(QApplication::activeModalWidget()))
        {
            EXPECT_EQ(progress->windowTitle(), qt::toQt(app::kSavingTitle));
            progress->cancel();
            canceled = true;
        }
    });
    h.save({.size = {4000, 3000}, .supersample = 4});
    EXPECT_TRUE(canceled);
    EXPECT_FALSE(std::filesystem::exists(*h.file));
    EXPECT_TRUE(h.errors.empty());
    EXPECT_FALSE(h.status(StatusField::RENDER).starts_with("Saved"));
}

TEST(ExportDialog, AnUnwritablePathIsReported)
{
    DialogHarness h;
    h.file = h.dir / "missing" / "directory" / "picture.png";
    h.save({.size = {40, 30}, .supersample = 1});
    ASSERT_EQ(h.errors.size(), 1U);
    EXPECT_TRUE(h.errors[0].starts_with("Save image failed: Could not save ")) << h.errors[0];
}

TEST(ExportDialog, IsModelessAndOpensOnce)
{
    DialogHarness h;
    h.window.action(Command::SAVE_IMAGE)->trigger();
    const qt::ExportDialog* dialog = h.window.exportDialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_FALSE(dialog->isModal());
    EXPECT_TRUE(dialog->isVisible());
    // The main window keeps working meanwhile.
    h.window.action(Command::ZOOM_IN)->trigger();
    EXPECT_EQ(h.status(StatusField::ZOOM), "Zoom 2x");
    h.window.app().showExportDialog();  // Try it: mandelbrotter:export
    EXPECT_EQ(h.window.exportDialog(), dialog);
    h.window.exportDialog()->reject();  // Cancel, Esc, the close box
    EXPECT_EQ(h.window.exportDialog(), nullptr);
}

TEST(ExportDialog, F1OpensItsHelpPage)
{
    DialogHarness h;
    h.window.showExportDialog();
    qt::ExportDialog* dialog = h.window.exportDialog();
    ASSERT_NE(dialog, nullptr);
    ASSERT_TRUE(mandelbrotter::test::activate(*dialog));
    dialog->widthField().setFocus();
    EXPECT_EQ(app::helpPageFor(h.window.helpContext()), app::kExportingPage);
    int helps      = 0;
    dialog->onHelp = [&helps] { ++helps; };
    QTest::keyClick(&dialog->widthField(), Qt::Key_F1);
    EXPECT_EQ(helps, 1);
}

// ---------------------------------------------------------------------------------------------------------------
// The other file items, the clipboard and bookmarks

TEST(ExportDialog, ViewsAreExportedAndImported)
{
    DialogHarness h;
    h.window.app().applySettings(app::seahorseFire());
    h.file = h.dir / "view.json";
    h.window.action(Command::EXPORT_VIEW)->trigger();
    ASSERT_TRUE(std::filesystem::exists(*h.file));
    h.window.app().applySettings(app::mandelbrotDefault());
    h.window.action(Command::IMPORT_VIEW)->trigger();
    EXPECT_EQ(h.window.app().settings(), app::seahorseFire());
    EXPECT_EQ(h.choices, (std::vector{qt::MainWindow::FileChoice::EXPORT_VIEW,
                                      qt::MainWindow::FileChoice::IMPORT_VIEW}));

    h.file = h.dir / "missing.json";
    h.window.action(Command::IMPORT_VIEW)->trigger();
    ASSERT_EQ(h.errors.size(), 1U);
    EXPECT_TRUE(h.errors[0].starts_with("Import view: ")) << h.errors[0];
}

TEST(ExportDialog, CtrlCInASeedFieldCopiesThePicture)
{
    DialogHarness h;
    h.activate();
    QLineEdit* seed = h.window.panel().controls().seedRe;
    seed->setFocus();
    QTest::keyClick(seed, Qt::Key_C, Qt::ControlModifier);
    const QImage copied = QGuiApplication::clipboard()->image();
    const auto   size   = h.window.canvas().controller().image().size();
    EXPECT_EQ(copied.size(), QSize(size.width, size.height));
    EXPECT_EQ(h.status(StatusField::RENDER), "Image copied to clipboard");
}

TEST(ExportDialog, AddBookmarkAsksForAName)
{
    DialogHarness            h;
    std::vector<std::string> suggestions;
    h.window.dialogSeams.askName = [&suggestions](std::string_view suggested) {
        suggestions.emplace_back(suggested);
        return std::optional<std::string>("my view");
    };
    h.window.action(Command::ADD_BOOKMARK)->trigger();
    EXPECT_EQ(suggestions, std::vector<std::string>{"Mandelbrot at 1x"});
    ASSERT_EQ(h.window.app().bookmarks().bookmarks().size(), 1U);
    EXPECT_EQ(h.window.app().bookmarks().bookmarks()[0].name, "my view");

    h.window.dialogSeams.askName = [](std::string_view) { return std::optional<std::string>(); };
    h.window.panel().controls().addBookmark->click();  // canceled
    EXPECT_EQ(h.window.app().bookmarks().bookmarks().size(), 1U);
}

}  // namespace
