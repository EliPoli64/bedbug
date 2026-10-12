#pragma once

#include "Id.h"

#include <QGraphicsItem>
#include <QPainterPath>

namespace bb {

class DiagramScene;

// The visual edge for one foreign-key relationship. Uses orthogonal routing between the
// child column port and the parent column port, then overlays crow's-foot markers at both
// ends (PLAN.md §8). Edges are re-routed when either endpoint moves.
class RelationshipItem : public QGraphicsItem
{
public:
    RelationshipItem(const Id &relationshipId, DiagramScene *scene,
                     QGraphicsItem *parent = nullptr);

    const Id &relationshipId() const { return m_relationshipId; }

    // Recomputes the route from the current scene/model state. Hides the item when the
    // underlying objects are missing.
    void refresh();

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
    bool m_childOptional = false;
};

} // namespace bb