#pragma once

#include <QLabel>
#include <QResizeEvent>
#include <QSize>
#include <QString>
#include <QWidget>

namespace mandelbrotter::qt
{

/// A status bar field: shows as much of its text as fits, with an ellipsis, and the whole text as
/// its tooltip. It never asks for more width than it gets, so a long text cannot widen the window.
class ElidedLabel : public QLabel
{
public:
    explicit ElidedLabel(QWidget* parent = nullptr);

    void                         setFullText(const QString& text);
    [[nodiscard]] const QString& fullText() const noexcept { return m_fullText; }

    [[nodiscard]] QSize minimumSizeHint() const override;
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void elide();

    QString m_fullText;
};

}  // namespace mandelbrotter::qt
