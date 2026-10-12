#pragma once

#include "EntityItem.h"
#include "Model.h"

#include <QGraphicsScene>
#include <QHash>

namespace bb {

// Owns the visual representation of one diagram in a Model. Items are rebuilt via
// syncFromModel(); item movement writes back to the diagram node positions (PLAN.md §8).
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

    // Rebuilds all EntityItems from the active diagram. Safe to call repeatedly.
    void syncFromModel();

    EntityItem *itemForEntity(const Id &entityId) const;

    // The active Diagram within m_model, or nullptr.
    Diagram *currentDiagram();

    bool snapToGrid() const { return m_snapToGrid; }
    void setSnapToGrid(bool enabled) { m_snapToGrid = enabled; }

signals:
    void entityMoved(const Id &entityId, const QPointF &pos);

private:
    Diagram *findDiagram();

    Model *m_model = nullptr;
    Id m_diagramId;
    QHash<Id, EntityItem *> m_items;
    bool m_snapToGrid = true;
};

} // namespace bb