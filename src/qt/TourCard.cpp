#include "qt/TourCard.h"

#include <QBoxLayout>
#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPushButton>
#include <QRect>
#include <QRectF>
#include <QWidget>
#include <cstddef>
#include <functional>
#include <string_view>

#include "Mandelbrotter/app/tour_layout.h"
#include "Mandelbrotter/app/ui_text.h"
#include "qt/qt_util.h"

namespace mandelbrotter::qt
{

namespace
{

/// Calls a callback that may be empty.
void call(const std::function<void()>& callback)
{
    if (callback)
    {
        callback();
    }
}

}  // namespace

TourCard::TourCard(QWidget* parent)
  : QFrame(parent),
    m_title(new QLabel(this)),
    m_text(new QLabel(this)),
    m_progress(new QLabel(this)),
    m_learnMore(new QPushButton(toQt(app::kLearnMore), this)),
    m_close(new QPushButton(toQt(app::kClose), this)),
    m_back(new QPushButton(toQt(app::kBack), this)),
    m_next(new QPushButton(toQt(app::kNext), this))
{
    // A tooltip's colors, like wx's info background.
    QPalette colors = palette();
    colors.setColor(QPalette::Window, colors.color(QPalette::ToolTipBase));
    colors.setColor(QPalette::WindowText, colors.color(QPalette::ToolTipText));
    setPalette(colors);
    setAutoFillBackground(true);

    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.2);
    m_title->setFont(titleFont);
    m_text->setWordWrap(true);
    m_text->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_text->setFixedWidth(app::kCardTextWidth);
    for (QLabel* label : {m_title, m_text, m_progress})
    {
        label->setTextFormat(Qt::PlainText);
    }

    connect(m_learnMore, &QPushButton::clicked, this, [this] { call(onLearnMore); });
    connect(m_close, &QPushButton::clicked, this, [this] { call(onClose); });
    connect(m_back, &QPushButton::clicked, this, [this] { call(onBack); });
    connect(m_next, &QPushButton::clicked, this, [this] { call(onNext); });

    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(app::kCardButtonGap);
    buttons->addWidget(m_learnMore);
    buttons->addWidget(m_close);
    buttons->addStretch(1);
    buttons->addWidget(m_back);
    buttons->addWidget(m_next);

    auto* layout = new QVBoxLayout(this);
    // Always the size its content asks for, the wrapped text's height included.
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setContentsMargins(app::kCardPadding, app::kCardPadding, app::kCardPadding,
                               app::kCardPadding);
    layout->setSpacing(app::kCardPadding);
    layout->addWidget(m_title);
    layout->addWidget(m_text);
    layout->addWidget(m_progress);
    layout->addLayout(buttons);
}

void TourCard::setStep(std::string_view title, std::string_view text, std::size_t index,
                       std::size_t count, bool hasHelpPage)
{
    m_title->setText(toQt(title));
    m_text->setText(toQt(text));
    fitText();
    m_progress->setText(toQt(app::stepText(index, count)));
    m_back->setEnabled(index > 0);
    m_next->setText(toQt(index + 1 == count ? app::kFinish : app::kNext));
    m_learnMore->setVisible(hasHelpPage);
    adjustSize();
}

void TourCard::fitText()
{
    // A wrapped label's size hint does not follow its fixed width: give it the height it needs, in
    // the font it has once the style has polished it (measured again once the card is shown).
    // heightForWidth() never answers less than the minimum height, so the last step's goes first.
    ensurePolished();
    m_text->ensurePolished();
    m_text->setMinimumHeight(0);
    m_text->setMaximumHeight(QWIDGETSIZE_MAX);
    m_text->setFixedHeight(m_text->heightForWidth(app::kCardTextWidth));
    adjustSize();
}

void TourCard::placeNear(const QRect& anchor)
{
    fitText();
    move(toQt(app::placeTourCard(fromQt(anchor), fromQt(size()), fromQt(parentWidget()->size()),
                                 app::kCardGap)));
    raise();
    show();
}

void TourCard::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);
    const double width = app::kCardBorder;
    QPainter     painter(this);
    painter.setPen(QPen(palette().color(QPalette::Highlight), width));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(rect()).adjusted(width / 2, width / 2, -width / 2, -width / 2));
}

}  // namespace mandelbrotter::qt
