#pragma once

#include "EntityItem.h"
#include "Model.h"

#include <QGraphicsScene>

#include <functional>
#include <optional>

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
    static constexpr qreal kSheetWidth = Diagram::kSheetWidth;
    static constexpr qreal kSheetHeight = Diagram::kSheetHeight;

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

    // Working-space ("sheet") grid for the active diagram.
    QRectF canvasRect() const;
    QRectF scrollableRect() const;   // canvas plus a panning margin on every side
    int pagesX() const;
    int pagesY() const;
    int originX() const;
    int originY() const;

    // Expands the sheet grid so it fully contains `rect` (grows by one sheet per
    // overflowing direction). Marks the document dirty via the callback.
    void expandCanvasToFit(const QRectF &rect);

    // Top-left for a new table of `size` centred on `preferredCenter` (scene
    // coordinates), nudged diagonally while it would overlap another table.
    // `exclude` skips the item being placed.
    QPointF freeTablePosition(const QPointF &preferredCenter, const QSizeF &size,
                              const Id &exclude = Id()) const;

    bool snapToGrid() const { return m_snapToGrid; }
    void setSnapToGrid(bool enabled) { m_snapToGrid = enabled; }

    // When enabled (the default) dragging a table past the sheet edge adds pages.
    bool autoGrowCanvas() const { return m_autoGrowCanvas; }
    void setAutoGrowCanvas(bool enabled) { m_autoGrowCanvas = enabled; }

    // Relationship tools (MySQL Workbench style): selecting a tool arms link mode for
    // relationship creation; std::nullopt disarms it. Esc cancels.
    void setLinkTool(std::optional<RelationshipKind> tool);
    std::optional<RelationshipKind> linkTool() const { return m_linkTool; }

    // Optional arrowhead at the parent end of every relationship edge.
    bool pointedLines() const { return m_pointedLines; }
    void setPointedLines(bool enabled) { m_pointedLines = enabled; }

    // Called by the shell whenever a relationship should be created from link mode.
    // Returns true when the relationship was actually created (drives the status text).
    void setRelationshipCreationHandler(
        std::function<bool(const Id &childEntityId, const QList<Id> &childColumnIds,
                           const Id &parentEntityId, const QList<Id> &parentColumnIds,
                           RelationshipKind kind)>
            handler);

    // Marks the document dirty without rebuilding (used by canvas auto-grow).
    void setMarkDirtyCallback(std::function<void()> callback);

    // Processes one link-mode click at a scene position. Returns true when handled.
    bool handleLinkAt(const QPointF &scenePos);

    // Processes a context-menu request at a scene position. Returns true when it landed
    // on a table (emitting tableContextMenuRequested). When it lands on empty space,
    // emits canvasContextMenuRequested instead.
    bool handleTableContextMenu(const QPointF &scenePos, const QPoint &globalPos);

signals:
    void entityMoved(const Id &entityId, const QPointF &pos);
    void linkStatusChanged(const QString &text);
    void tableContextMenuRequested(const Id &entityId, const QPoint &globalPos);
    void canvasContextMenuRequested(const QPoint &globalPos);
    void canvasChanged();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    Diagram *findDiagram();
    void refreshRelationships();
    void beginOrCompleteLink(const Id &entityId, const Id &columnId);

    Model *m_model = nullptr;
    Id m_diagramId;
    QHash<Id, EntityItem *> m_items;
    QHash<Id, RelationshipItem *> m_relationshipItems;
    bool m_snapToGrid = false;   // Workbench-like free dragging by default
    bool m_autoGrowCanvas = true;

    std::optional<RelationshipKind> m_linkTool;
    bool m_pointedLines = false;
    Id m_linkSourceEntity;
    Id m_linkSourceColumn;
    std::function<bool(const Id &, const QList<Id> &, const Id &, const QList<Id> &,
                       RelationshipKind)>
        m_relationshipHandler;
    std::function<void()> m_markDirtyCallback;
};

} // namespace bb