#include "qt/ExportDialog.h"

#include <QBoxLayout>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QKeySequence>
#include <QLabel>
#include <QShortcut>
#include <QSpinBox>
#include <QWidget>
#include <algorithm>
#include <cstddef>

#include "Mandelbrotter/app/ui_text.h"
#include "Mandelbrotter/exporter.h"
#include "qt/qt_util.h"

namespace mandelbrotter::qt
{

ExportDialog::ExportDialog(QWidget* parent, PixelSize initialSize)
  : QDialog(parent),
    m_width(new QSpinBox(this)),
    m_height(new QSpinBox(this)),
    m_supersample(new QComboBox(this))
{
    setWindowTitle(toQt(app::kExportDialogTitle));
    setModal(false);
    setSizeGripEnabled(true);

    for (QSpinBox* field : {m_width, m_height})
    {
        field->setRange(1, kMaxExportDimension);
    }
    m_width->setValue(std::clamp(initialSize.width, 1, kMaxExportDimension));
    m_height->setValue(std::clamp(initialSize.height, 1, kMaxExportDimension));
    for (const app::SupersampleChoice& choice : app::kSupersampleChoices)
    {
        m_supersample->addItem(toQt(choice.label));
    }

    auto* form = new QFormLayout();
    form->addRow(toQt(app::kWidthLabel), m_width);
    form->addRow(toQt(app::kHeightLabel), m_height);
    form->addRow(toQt(app::kAntiAliasingLabel), m_supersample);
    auto* note = new QLabel(toQt(app::kExportNote), this);
    note->setWordWrap(true);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(note);
    layout->addWidget(buttons);

    // F1 opens the page about saving pictures (the main window's F1 does not reach a dialog).
    new QShortcut(
        QKeySequence(Qt::Key_F1), this,
        [this] {
            if (onHelp)
            {
                onHelp();
            }
        },
        Qt::WidgetWithChildrenShortcut);
}

ExportOptions ExportDialog::options() const
{
    const int choice = std::clamp(m_supersample->currentIndex(), 0,
                                  static_cast<int>(app::kSupersampleChoices.size()) - 1);
    return {
        .size        = {m_width->value(), m_height->value()},
        .supersample = app::kSupersampleChoices.at(static_cast<std::size_t>(choice)).factor,
    };
}

}  // namespace mandelbrotter::qt
