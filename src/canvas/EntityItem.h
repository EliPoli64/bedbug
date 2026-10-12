#pragma once

#include "CanonicalType.h"
#include "Column.h"
#include "Entity.h"
#include "Id.h"

#include <QGraphicsItem>

#include <functional>

namespace bb {

class DiagramScene;

// A table on the canvas. Paints the header bar and one row per column (PK/unique badge,
// name, mapped type, not-null marker). Rows are painted directly; hit-testing goes through
// columnAt() for port math (PLAN.md §8).
class EntityItem : public QGraphicsItem
{
public:
    static constexpr qreal kHeaderHeight = 24.0;
    static constexpr qreal kRowHeight = 18.0;
    static constexpr qreal kHPadding = 8.0;
    static constexpr qreal kBadgeWidth = 26.0;

    explicit EntityItem(const Entity *entity, QGraphicsItem *parent = nullptr);

    const Entity *entity() const { return m_entity; }
    const Id &entityId() const { return m_entity->id; }

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

    // Row index (0-based) at an item-local position, or -1 when inside the header or
    // outside the body.
    int columnAt(const QPointF &itemPos) const;

    // Scene-space port points on the requested edges, centered vertically in the row.
    QPointF leftPortFor(const Id &columnId) const;
    QPointF rightPortFor(const Id &columnId) const;

    // Called by DiagramScene; fires whenever the item moves (after snapping).
    void setOnMoved(std::function<void(const QPointF &pos)> callback);

    qreal contentWidth() const;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    int indexOfColumn(const Id &columnId) const;

    const Entity *m_entity = nullptr;
    qreal m_width = 140.0;
    std::function<void(const QPointF &pos)> m_onMoved;
};

} // namespace bb