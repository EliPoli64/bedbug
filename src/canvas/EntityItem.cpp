#include "EntityItem.h"

#include "DiagramScene.h"

#include <QFontMetricsF>
#include <QGraphicsScene>
#include <QPainter>
#include <QPalette>
#include <QStyleOptionGraphicsItem>

#include <cmath>

namespace bb {

namespace {

QString columnTypeText(const CanonicalType &type)
{
    using K = CanonicalType::Kind;
    switch (type.kind) {
    case K::Varchar:
    case K::Char:
        return type.length > 0
                   ? CanonicalType::kindToString(type.kind).toLower() + QStringLiteral("(%1)")
                         .arg(type.length)
                   : CanonicalType::kindToString(type.kind).toLower();
    case K::Decimal:
        return type.precision > 0
                   ? QStringLiteral("decimal(%1,%2)").arg(type.precision).arg(type.scale)
                   : QStringLiteral("decimal");
    case K::Enum:
        return QStringLiteral("enum");
    default:
        return CanonicalType::kindToString(type.kind).toLower();
    }
}

} // namespace

EntityItem::EntityItem(const Entity *entity, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_entity(entity)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    if (m_entity) {
        QFontMetricsF metrics{QFont()};
        m_width = 180.0;
        m_width = qMax(m_width, metrics.horizontalAdvance(m_entity->name) + kHPadding * 2);
        for (const Column &column : m_entity->columns) {
            const qreal rowText = metrics.horizontalAdvance(column.name) + kBadgeWidth
                                  + metrics.horizontalAdvance(columnTypeText(column.type))
                                  + kHPadding * 2 + 8.0;
            m_width = qMax(m_width, rowText);
        }
        m_width = qMin(m_width, 420.0);
    }
}

qreal EntityItem::contentWidth() const
{
    return m_width;
}

QRectF EntityItem::boundingRect() const
{
    const qreal height = kHeaderHeight + (m_entity ? m_entity->columns.size() : 0) * kRowHeight;
    return QRectF(0, 0, m_width, height);
}

void EntityItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (!m_entity)
        return;

    const QRectF rect = boundingRect();
    const QPalette palette = option ? option->palette : QPalette();

    const bool selected = option && (option->state & QStyle::State_Selected);

    // Body background.
    painter->setPen(QPen(palette.color(QPalette::Mid), 1));
    painter->setBrush(palette.color(QPalette::Base));
    painter->drawRect(rect);

    // Header bar.
    const QRectF headerRect(0, 0, m_width, kHeaderHeight);
    painter->fillRect(headerRect, selected ? palette.color(QPalette::Dark)
                                           : palette.color(QPalette::Highlight));
    painter->setPen(palette.color(QPalette::HighlightedText));
    QFont headerFont = painter->font();
    headerFont.setBold(true);
    painter->setFont(headerFont);
    painter->drawText(headerRect, Qt::AlignCenter, m_entity->name);

    // Body font (non-bold).
    QFont bodyFont = painter->font();
    bodyFont.setBold(false);
    painter->setFont(bodyFont);

    const QFontMetricsF metrics = painter->fontMetrics();
    QColor mutedColor = palette.color(QPalette::WindowText);
    mutedColor.setAlpha(140);

    qreal y = kHeaderHeight;
    for (int i = 0; i < m_entity->columns.size(); ++i) {
        const Column &column = m_entity->columns.at(i);
        const QRectF rowRect(0, y, m_width, kRowHeight);

        if (selected) {
            painter->fillRect(rowRect, palette.color(QPalette::Highlight).lighter(135));
        } else if (i % 2 == 1) {
            painter->fillRect(rowRect, palette.color(QPalette::AlternateBase));
        }

        // Badge (PK / U).
        const QRectF badgeRect(kHPadding, y, kBadgeWidth, kRowHeight);
        painter->setPen(palette.color(QPalette::WindowText));
        if (column.primaryKey)
            painter->drawText(badgeRect, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("PK"));
        else if (column.unique)
            painter->drawText(badgeRect, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("U"));

        // Column name.
        QRectF nameRect = badgeRect;
        nameRect.setWidth(m_width - kHPadding * 2 - kBadgeWidth - 8.0);
        painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, column.name);

        // Type, right-aligned, muted.
        const QString typeText = columnTypeText(column.type);
        const qreal typeWidth = metrics.horizontalAdvance(typeText) + 4.0;
        QRectF typeRect(m_width - kHPadding - typeWidth, y, typeWidth, kRowHeight);
        painter->setPen(mutedColor);
        painter->drawText(typeRect, Qt::AlignRight | Qt::AlignVCenter, typeText);

        // Not-null marker.
        if (!column.nullable) {
            painter->setPen(palette.color(QPalette::WindowText));
            const QRectF nnRect(m_width - kHPadding - 3.0, y, 4.0, kRowHeight);
            painter->drawText(nnRect, Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("•"));
        }

        y += kRowHeight;
    }
}

int EntityItem::columnAt(const QPointF &itemPos) const
{
    if (!m_entity)
        return -1;
    const qreal bodyTop = kHeaderHeight;
    const qreal bodyBottom = bodyTop + m_entity->columns.size() * kRowHeight;
    if (itemPos.y() < bodyTop || itemPos.y() >= bodyBottom)
        return -1;
    return static_cast<int>((itemPos.y() - bodyTop) / kRowHeight);
}

QPointF EntityItem::leftPortFor(const Id &columnId) const
{
    const int index = indexOfColumn(columnId);
    if (index < 0)
        return QPointF();
    const qreal y = kHeaderHeight + index * kRowHeight + kRowHeight / 2.0;
    return mapToScene(QPointF(0, y));
}

QPointF EntityItem::rightPortFor(const Id &columnId) const
{
    const int index = indexOfColumn(columnId);
    if (index < 0)
        return QPointF();
    const qreal y = kHeaderHeight + index * kRowHeight + kRowHeight / 2.0;
    return mapToScene(QPointF(m_width, y));
}

void EntityItem::setOnMoved(std::function<void(const QPointF &pos)> callback)
{
    m_onMoved = std::move(callback);
}

int EntityItem::indexOfColumn(const Id &columnId) const
{
    if (!m_entity)
        return -1;
    for (int i = 0; i < m_entity->columns.size(); ++i) {
        if (m_entity->columns.at(i).id == columnId)
            return i;
    }
    return -1;
}

QVariant EntityItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && m_entity) {
        QPointF wanted = value.toPointF();
        if (auto *scene = dynamic_cast<DiagramScene *>(this->scene())) {
            if (scene->snapToGrid()) {
                constexpr qreal grid = DiagramScene::kGridSize;
                wanted = QPointF(std::round(wanted.x() / grid) * grid,
                                 std::round(wanted.y() / grid) * grid);
            }
        }
        return wanted;
    }

    if (change == ItemPositionHasChanged && m_onMoved)
        m_onMoved(value.toPointF());

    return QGraphicsItem::itemChange(change, value);
}

} // namespace bb