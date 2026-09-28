#pragma once

#include <QComboBox>
#include <QDialog>
#include <QSpinBox>
#include <QWidget>
#include <functional>

#include "Mandelbrotter/exporter.h"
#include "Mandelbrotter/geometry.h"

namespace mandelbrotter::qt
{

/// Asks for the output size and anti-aliasing level of a PNG export. Shown modeless by MainWindow,
/// which handles its OK and Cancel.
class ExportDialog : public QDialog
{
public:
    ExportDialog(QWidget* parent, PixelSize initialSize);

    [[nodiscard]] ExportOptions options() const;

    [[nodiscard]] QSpinBox&  widthField() const noexcept { return *m_width; }
    [[nodiscard]] QSpinBox&  heightField() const noexcept { return *m_height; }
    [[nodiscard]] QComboBox& supersampleField() const noexcept { return *m_supersample; }

    /// F1 inside the dialog.
    std::function<void()> onHelp;

private:
    QSpinBox*  m_width{nullptr};
    QSpinBox*  m_height{nullptr};
    QComboBox* m_supersample{nullptr};
};

}  // namespace mandelbrotter::qt
