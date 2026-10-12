#pragma once

#include "EntityItem.h"
#include "Model.h"

#include <QGraphicsScene>

#include <functional>

namespace bb {

class RelationshipItem;

// Owns the visual representation of one diagram in a Model. Items are rebuilt via
// syncFromModel(); item movement writes back to the diagram node positions and refreshes
// relationship edges (PLAN.md §8).
class DiagramScene : public QGraphicsScene
{
    Q_OBJECT

public:
    static constexpr qreal kGridSize = 20.0;

    explicit DiagramScene(QObject *parent = nullptr);

    void setModel(Model *model);
    Model *model() const { return m_model; }

    void setActiveDiagramId(const Id &diagramId);
    const Id &activeDiagramId() const { return m_diagramId; }

    // Rebuilds all EntityItems and RelationshipItems from the active diagram.
    void syncFromModel();

    EntityItem *itemForEntity(const Id &entityId) const;
    RelationshipItem *itemForRelationship(const Id &relationshipId) const;

    // The active Diagram within m_model, or nullptr.
    Diagram *currentDiagram();

    bool snapToGrid() const { return m_snapToGrid; }
    void setSnapToGrid(bool enabled) { m_snapToGrid = enabled; }

    // Link mode: clicking a source column then a column on a different entity creates a
    // relationship via the handler. Esc / toggling off cancels.
    void setLinkMode(bool enabled);
    bool linkMode() const { return m_linkMode; }
    void setRelationshipCreationHandler(
        std::function<void(const Id &childEntityId, const QList<Id> &childColumnIds,
                           const Id &parentEntityId, const QList<Id> &parentColumnIds)>
            handler);

    // Processes one link-mode click at a scene position. Returns true when handled.
    bool handleLinkAt(const QPointF &scenePos);

signals:
    void entityMoved(const Id &entityId, const QPointF &pos);
    void linkStatusChanged(const QString &text);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    Diagram *findDiagram();
    void refreshRelationships();
    void beginOrCompleteLink(const Id &entityId, const Id &columnId);

    Model *m_model = nullptr;
    Id m_diagramId;
    QHash<Id, EntityItem *> m_items;
    QHash<Id, RelationshipItem *> m_relationshipItems;
    bool m_snapToGrid = true;

    bool m_linkMode = false;
    Id m_linkSourceEntity;
    Id m_linkSourceColumn;
    std::function<void(const Id &, const QList<Id> &, const Id &, const QList<Id> &)>
        m_relationshipHandler;
};

} // namespace bb