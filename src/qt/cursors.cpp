#include "qt/cursors.h"

#include <QColor>
#include <QCursor>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <utility>

namespace mandelbrotter::qt
{

QCursor bullseyeCursor()
{
    constexpr int    kSize = 32;
    constexpr double kMid  = kSize / 2.0;
    QPixmap          pixmap(kSize, kSize);
    pixmap.fill(Qt::transparent);
    {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::NoBrush);
        const QPointF center(kMid, kMid);
        // Each shape in black under white, so it shows on any picture.
        for (const auto& [color, width] :
             {std::pair{QColor(Qt::black), 3.0}, std::pair{QColor(Qt::white), 1.0}})
        {
            painter.setPen(QPen(color, width));
            painter.drawEllipse(center, 11.0, 11.0);
            painter.drawEllipse(center, 6.0, 6.0);
            painter.drawEllipse(center, 1.5, 1.5);
        }
    }
    return QCursor(pixmap, kSize / 2, kSize / 2);
}

}  // namespace mandelbrotter::qt
