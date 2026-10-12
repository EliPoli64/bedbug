#include "RelationshipItem.h"

#include "CrowsFootPainter.h"
#include "DiagramScene.h"
#include "EntityItem.h"

#include <QPainter>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>
#include <QVector>

namespace bb {

namespace {

constexpr qreal kDetourGap = 60.0;
constexpr qreal kMinOverlapJog = 24.0;

// Frees up an orthogonal polyline from s to e that clears the two entity boxes.
QVector<QPointF> routeOrthogonal(const QPointF &s, const QPointF &e,
                                 const QRectF &childBounds, const QRectF &parentBounds)
{
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

    const bool parentRightOfChild = parentItem->pos().x() >= childItem->pos().x();
    m_childAnchor = parentRightOfChild ? childItem->rightPortFor(childColumn->id)
                                       : childItem->leftPortFor(childColumn->id);
    m_parentAnchor = parentRightOfChild ? parentItem->leftPortFor(parentColumn->id)
                                        : parentItem->rightPortFor(parentColumn->id);

    const QRectF childBounds = childItem->mapToScene(childItem->boundingRect()).boundingRect();
    const QRectF parentBounds = parentItem->mapToScene(parentItem->boundingRect()).boundingRect();

    QVector<QPointF> points = routeOrthogonal(m_childAnchor, m_parentAnchor, childBounds, parentBounds);

    // Optionality on the child end: derived from the FK columns' nullability.
    m_childOptional = false;
    for (const Id &columnId : relationship->childColumns) {
        const Column *column = childItem->entity()->findColumn(columnId);
        if (column && column->nullable) {
            m_childOptional = true;
            break;
        }
    }

    const qreal childTrim = CrowsFootPainter::markerZoneLength(Cardinality::Many, m_childOptional);
    const qreal parentTrim = CrowsFootPainter::markerZoneLength(Cardinality::One, false);

    m_outwardFromChild = normalizedDelta(points.first(), points.value(1));
    m_outwardFromParent = normalizedDelta(points.last(), points.value(points.size() - 2));

    // Inset the two ends so the markers own the final segments.
    if (!m_outwardFromChild.isNull())
        points[0] = points.first() + m_outwardFromChild * childTrim;
    if (!m_outwardFromParent.isNull())
        points[points.size() - 1] = points.last() + m_outwardFromParent * parentTrim;

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

    painter->setPen(QPen(color, selected ? 2.0 : 1.2));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_path);

    CrowsFootPainter::paint(painter, m_childAnchor, m_outwardFromChild, Cardinality::Many,
                            m_childOptional);
    CrowsFootPainter::paint(painter, m_parentAnchor, m_outwardFromParent, Cardinality::One,
                            false);
}

} // namespace bb