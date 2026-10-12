#pragma once

#include "CrowsFootPainter.h"
#include "Id.h"

#include <QGraphicsItem>
#include <QPainterPath>

namespace bb {

class DiagramScene;

// The visual edge for one foreign-key relationship. Uses orthogonal routing between the
// child column port and the parent column port, then overlays crow's-foot markers at both
// ends (PLAN.md §8). Edges are re-routed when either endpoint moves. Line style follows
// identifying (solid) vs non-identifying (dashed); the child end renders "one" instead of
// the crow's foot when all child FK columns are unique (1:1).
class RelationshipItem : public QGraphicsItem
{
public:
    RelationshipItem(const Id &relationshipId, DiagramScene *scene,
                     QGraphicsItem *parent = nullptr);

    const Id &relationshipId() const { return m_relationshipId; }

    // Recomputes the route from the current scene/model state. Hides the item when the
    // underlying objects are missing.
    void refresh();

    // Which marker is drawn at the child end after the last refresh.
    Cardinality childCardinality() const { return m_childCardinality; }

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

private:
    Id m_relationshipId;
    DiagramScene *m_scene = nullptr;

    QPainterPath m_path;

    // End anchors and directions for the marker paint pass.
    QPointF m_childAnchor;
    QPointF m_parentAnchor;
    QPointF m_outwardFromChild;
    QPointF m_outwardFromParent;
    Cardinality m_childCardinality = Cardinality::Many;
    bool m_childOptional = false;
    bool m_identifying = false;

    // Optional direction arrow, in the last segment before the parent end.
    QPointF m_arrowPos;
    QPointF m_arrowDir;
};

} // namespace bb