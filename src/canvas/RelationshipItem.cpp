#include "RelationshipItem.h"

#include "CrowsFootPainter.h"
#include "DiagramScene.h"
#include "EntityItem.h"

#include <QPainter>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QStyleOptionGraphicsItem>
#include <QVector>

namespace bb {

namespace {

constexpr qreal kDetourGap = 60.0;
constexpr qreal kMinOverlapJog = 24.0;

// Frees up an orthogonal polyline from s to e that clears the two entity boxes.
QVector<QPointF> routeOrthogonal(const QPointF &s, const QPointF &e,
                                 const QRectF &childBounds, const QRectF &parentBounds,
                                 bool vertical)
{
    if (vertical) {
        // Stacked boxes have a clear lane between them. When they overlap vertically,
        // detour sideways instead so the line does not cut through a table.
        const bool verticalGap = childBounds.bottom() < parentBounds.top()
                                 || parentBounds.bottom() < childBounds.top();
        if (verticalGap) {
            const qreal midY = (s.y() + e.y()) / 2.0;
            return {s, QPointF(s.x(), midY), QPointF(e.x(), midY), e};
        }

        const qreal left = qMin(childBounds.left(), parentBounds.left());
        const qreal right = qMax(childBounds.right(), parentBounds.right());
        const qreal midX = (left >= kDetourGap) ? left - kDetourGap : right + kDetourGap;
        return {s, QPointF(midX, s.y()), QPointF(midX, e.y()), e};
    }

    const bool parentRightOfChild = e.x() >= s.x();
    const qreal childInner = parentRightOfChild ? childBounds.right() : childBounds.left();
    const qreal parentInner = parentRightOfChild ? parentBounds.left() : parentBounds.right();

    const bool horizontalGap = parentRightOfChild ? parentInner > childInner + kMinOverlapJog
                                                  : childInner > parentInner + kMinOverlapJog;

    if (horizontalGap) {
        // S -> (midX, Sy) -> (midX, Ey) -> E
        const qreal midX = (childInner + parentInner) / 2.0;
        return {s, QPointF(midX, s.y()), QPointF(midX, e.y()), e};
    }

    // Boxes overlap horizontally: detour above or below the taller footprint.
    const qreal top = qMin(childBounds.top(), parentBounds.top());
    const qreal bottom = qMax(childBounds.bottom(), parentBounds.bottom());
    const qreal midY = (top >= kDetourGap) ? top - kDetourGap : bottom + kDetourGap;
    return {s, QPointF(s.x(), midY), QPointF(e.x(), midY), e};
}

struct SidePair
{
    EntityItem::Side child;
    EntityItem::Side parent;
};

// Pick the pair of facing table edges from how the two boxes are arranged. Stacked
// tables connect bottom-to-top (or top-to-bottom); side-by-side tables connect
// right-to-left (or left-to-right).
SidePair facingSides(const QRectF &childBounds, const QRectF &parentBounds)
{
    const QPointF delta = parentBounds.center() - childBounds.center();

    const bool xOverlap = childBounds.left() <= parentBounds.right()
                          && parentBounds.left() <= childBounds.right();
    const bool yOverlap = childBounds.top() <= parentBounds.bottom()
                          && parentBounds.top() <= childBounds.bottom();

    bool vertical;
    if (yOverlap && !xOverlap)
        vertical = false;   // side by side
    else if (xOverlap && !yOverlap)
        vertical = true;    // stacked
    else
        vertical = qAbs(delta.y()) > qAbs(delta.x());

    if (vertical) {
        return delta.y() >= 0 ? SidePair{EntityItem::Side::Bottom, EntityItem::Side::Top}
                              : SidePair{EntityItem::Side::Top, EntityItem::Side::Bottom};
    }
    return delta.x() >= 0 ? SidePair{EntityItem::Side::Right, EntityItem::Side::Left}
                          : SidePair{EntityItem::Side::Left, EntityItem::Side::Right};
}

QPointF normalizedDelta(const QPointF &from, const QPointF &to)
{
    const QPointF delta = to - from;
    return qFuzzyIsNull(delta.manhattanLength()) ? QPointF() : delta / qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
}

} // namespace

RelationshipItem::RelationshipItem(const Id &relationshipId, DiagramScene *scene,
                                   QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_relationshipId(relationshipId)
    , m_scene(scene)
{
    setAcceptHoverEvents(false);
}

void RelationshipItem::refresh()
{
    if (!m_scene || !m_scene->model()) {
        setVisible(false);
        return;
    }

    const Model *model = m_scene->model();
    const Relationship *relationship = nullptr;
    for (const Relationship &candidate : model->relationships) {
        if (candidate.id == m_relationshipId) {
            relationship = &candidate;
            break;
        }
    }
    if (!relationship) {
        setVisible(false);
        return;
    }

    EntityItem *childItem = m_scene->itemForEntity(relationship->childEntity);
    EntityItem *parentItem = m_scene->itemForEntity(relationship->parentEntity);
    if (!childItem || !parentItem || relationship->childColumns.isEmpty()
        || relationship->parentColumns.isEmpty()) {
        setVisible(false);
        return;
    }

    const Column *childColumn = childItem->entity()->findColumn(relationship->childColumns.first());
    const Column *parentColumn = parentItem->entity()->findColumn(relationship->parentColumns.first());
    if (!childColumn || !parentColumn) {
        setVisible(false);
        return;
    }

    const QRectF childBounds = childItem->mapToScene(childItem->boundingRect()).boundingRect();
    const QRectF parentBounds = parentItem->mapToScene(parentItem->boundingRect()).boundingRect();

    const SidePair sides = facingSides(childBounds, parentBounds);
    m_childAnchor = childItem->portFor(childColumn->id, sides.child);
    m_parentAnchor = parentItem->portFor(parentColumn->id, sides.parent);

    const bool vertical = sides.child == EntityItem::Side::Top
                          || sides.child == EntityItem::Side::Bottom;
    QVector<QPointF> points =
        routeOrthogonal(m_childAnchor, m_parentAnchor, childBounds, parentBounds, vertical);

    // Optionality on the child end: derived from the FK columns' nullability.
    m_childOptional = false;
    for (const Id &columnId : relationship->childColumns) {
        const Column *column = childItem->entity()->findColumn(columnId);
        if (column && column->nullable) {
            m_childOptional = true;
            break;
        }
    }

    // Cardinality on the child end: all FK columns unique (1:1) -> a "one" marker,
    // otherwise the crow's foot (1:N).
    m_childCardinality = Cardinality::Many;
    for (const Id &columnId : relationship->childColumns) {
        const Column *column = childItem->entity()->findColumn(columnId);
        if (!column || !column->unique) {
            m_childCardinality = Cardinality::Many;
            break;
        }
        m_childCardinality = Cardinality::One;   // still one when the loop finishes
    }

    m_identifying = relationship->identifying;

    const qreal childTrim = CrowsFootPainter::markerZoneLength(m_childCardinality, m_childOptional);
    const qreal parentTrim = CrowsFootPainter::markerZoneLength(Cardinality::One, false);

    m_outwardFromChild = normalizedDelta(points.first(), points.value(1));
    m_outwardFromParent = normalizedDelta(points.last(), points.value(points.size() - 2));

    // Inset the two ends so the markers own the final segments.
    if (!m_outwardFromChild.isNull())
        points[0] = points.first() + m_outwardFromChild * childTrim;
    if (!m_outwardFromParent.isNull())
        points[points.size() - 1] = points.last() + m_outwardFromParent * parentTrim;

    // Direction arrow for "pointed" lines sits mid-way along the trimmed final segment.
    m_arrowPos = QPointF();
    if (m_scene && m_scene->pointedLines() && points.size() >= 2
        && !m_outwardFromParent.isNull()) {
        const QPointF lastBefore = points.at(points.size() - 2);
        const QPointF trimmedEnd = points.last();
        m_arrowPos = (lastBefore + trimmedEnd) / 2.0;
        m_arrowDir = normalizedDelta(lastBefore, trimmedEnd);
    }

    QPainterPath path(points.first());
    for (int i = 1; i < points.size(); ++i)
        path.lineTo(points.at(i));

    prepareGeometryChange();
    m_path = path;
    setVisible(true);
    update();
}

QRectF RelationshipItem::boundingRect() const
{
    return m_path.boundingRect().adjusted(-24, -24, 24, 24);
}

QPainterPath RelationshipItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(8.0);
    return stroker.createStroke(m_path);
}

void RelationshipItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (m_path.isEmpty())
        return;

    const bool selected = option && (option->state & QStyle::State_Selected);
    QColor color = option ? option->palette.color(QPalette::WindowText) : Qt::black;
    if (selected)
        color = option->palette.color(QPalette::Highlight);

    QPen pen(color, selected ? 2.2 : 1.4);
    pen.setJoinStyle(Qt::RoundJoin);   // smooth rounded turns at route corners
    pen.setCapStyle(Qt::RoundCap);
    // Identifying relationships are solid; non-identifying are dashed.
    if (!selected && !m_identifying)
        pen.setStyle(Qt::DashLine);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_path);

    CrowsFootPainter::paint(painter, m_childAnchor, m_outwardFromChild, m_childCardinality,
                            m_childOptional);
    CrowsFootPainter::paint(painter, m_parentAnchor, m_outwardFromParent, Cardinality::One,
                            false);

    // Optional direction arrow for "pointed" lines.
    if (!m_arrowPos.isNull() && !m_arrowDir.isNull() && !selected) {
        const QPointF perp(-m_arrowDir.y(), m_arrowDir.x());
        const qreal arrowLength = 9.0;
        const qreal arrowHalfWidth = 3.2;
        const QPointF base = m_arrowPos - m_arrowDir * arrowLength;
        const QPointF tip = m_arrowPos + m_arrowDir * arrowLength;
        QPolygonF arrow;
        arrow << base + perp * arrowHalfWidth << tip << base - perp * arrowHalfWidth;
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawPolygon(arrow);
    }
}

} // namespace bb