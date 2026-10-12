#pragma once

#include <QPainter>
#include <QPointF>

namespace bb {

enum class Cardinality
{
    One,   // perpendicular tick       ("one and only one")
    Many,  // three diverging prongs   ("one or more")
};

// Crow's-foot (IE) notation markers, painted at the end of a relationship edge,
// radiating away from the entity they belong to. (PLAN.md §8)
namespace CrowsFootPainter {

inline constexpr qreal kCircleRadius = 4.0;
inline constexpr qreal kProngLength = 16.0;
inline constexpr qreal kTickGap = 3.0;
inline constexpr qreal kTickHalfLength = 6.0;

// The length of the marker zone measured from the entity border, along `direction`.
inline qreal markerZoneLength(Cardinality cardinality, bool optional)
{
    qreal length = optional ? (2.0 * kCircleRadius + 2.0) : 0.0;
    length += (cardinality == Cardinality::Many) ? kProngLength : kTickGap + 4.0;
    return length;
}

// Paints a cardinality/optionality symbol at `anchor`, radiating along `direction`
// (assumed normalized, pointing away from the entity). Uses the painter's current pen.
void paint(QPainter *painter, const QPointF &anchor, const QPointF &direction,
           Cardinality cardinality, bool optional);

} // namespace CrowsFootPainter

} // namespace bb