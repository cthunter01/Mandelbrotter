#include "qt/ElidedLabel.h"

#include <QFontMetrics>
#include <QLabel>
#include <QResizeEvent>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QWidget>

namespace mandelbrotter::qt
{

ElidedLabel::ElidedLabel(QWidget* parent) : QLabel(parent)
{
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    setTextFormat(Qt::PlainText);
}

void ElidedLabel::setFullText(const QString& text)
{
    if (text == m_fullText)
    {
        return;
    }
    m_fullText = text;
    setToolTip(text);
    elide();
}

QSize ElidedLabel::minimumSizeHint() const
{
    return {0, QLabel::minimumSizeHint().height()};
}

QSize ElidedLabel::sizeHint() const
{
    return {fontMetrics().horizontalAdvance(m_fullText), QLabel::sizeHint().height()};
}

void ElidedLabel::resizeEvent(QResizeEvent* event)
{
    QLabel::resizeEvent(event);
    elide();
}

void ElidedLabel::elide()
{
    setText(fontMetrics().elidedText(m_fullText, Qt::ElideRight, contentsRect().width()));
}

}  // namespace mandelbrotter::qt
