#include "CrowsFootPainter.h"

#include <QLineF>
#include <QtMath>

namespace bb {

namespace CrowsFootPainter {

namespace {

QPointF rotated(const QPointF &v, qreal degrees)
{
    const qreal radians = qDegreesToRadians(degrees);
    const qreal c = qCos(radians);
    const qreal s = qSin(radians);
    return QPointF(v.x() * c - v.y() * s, v.x() * s + v.y() * c);
}

} // namespace

void paint(QPainter *painter, const QPointF &anchor, const QPointF &direction,
           Cardinality cardinality, bool optional)
{
    qreal offset = 0.0;
    if (optional) {
        painter->drawEllipse(anchor + direction * kCircleRadius, kCircleRadius, kCircleRadius);
        offset = 2.0 * kCircleRadius + 2.0;
    }

    const QPointF start = anchor + direction * offset;

    if (cardinality == Cardinality::Many) {
        // Three prongs diverging from the entity.
        painter->drawLine(QLineF(start, start + direction * kProngLength));
        painter->drawLine(QLineF(start, start + rotated(direction, 30.0) * kProngLength));
        painter->drawLine(QLineF(start, start + rotated(direction, -30.0) * kProngLength));
    } else {
        // The "one" side: a single perpendicular bar when optional, two parallel bars
        // when mandatory ("one and only one").
        const int tickCount = optional ? 1 : 2;
        const QPointF perpendicular(-direction.y(), direction.x());
        for (int i = 0; i < tickCount; ++i) {
            const QPointF tickCenter = start + direction * (kTickGap + i * 3.0);
            painter->drawLine(QLineF(tickCenter - perpendicular * kTickHalfLength,
                                     tickCenter + perpendicular * kTickHalfLength));
        }
    }
}

} // namespace CrowsFootPainter

} // namespace bb