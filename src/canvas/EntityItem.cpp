#include "EntityItem.h"

#include "DiagramScene.h"

#include <QApplication>
#include <QFontMetricsF>
#include <QGraphicsScene>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QStyleOptionGraphicsItem>

#include <cmath>

namespace bb {

namespace {

// Header bar with rounded top corners, matching the box's corner radius.
QPainterPath roundedTopPath(const QRectF &rect, qreal radius)
{
    QPainterPath path;
    path.moveTo(rect.left(), rect.bottom());
    path.lineTo(rect.left(), rect.top() + radius);
    path.quadTo(rect.left(), rect.top(), rect.left() + radius, rect.top());
    path.lineTo(rect.right() - radius, rect.top());
    path.quadTo(rect.right(), rect.top(), rect.right(), rect.top() + radius);
    path.lineTo(rect.right(), rect.bottom());
    path.closeSubpath();
    return path;
}

} // namespace

EntityItem::EntityItem(const Entity *entity, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_entity(entity)
{
    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);

    if (m_entity) {
        // Measure with the application font, which is what QGraphicsView paints with by
        // default.
        QFontMetricsF metrics{QApplication::font()};
        QFont titleFont = QApplication::font();
        titleFont.setBold(true);
        QFontMetricsF titleMetrics{titleFont};

        m_width = 180.0;
        m_width = qMax(m_width, titleMetrics.horizontalAdvance(m_entity->name) + kHPadding * 2);
        for (const Column &column : m_entity->columns) {
            const qreal rowText = metrics.horizontalAdvance(column.name)
                                  + metrics.horizontalAdvance(column.type.displayName())
                                  + kBadgeWidth + kHPadding * 2 + 12.0;
            m_width = qMax(m_width, rowText);
        }
        m_width = qMin(m_width, 440.0);

        QStringList tooltip;
        tooltip << m_entity->name;
        if (!m_entity->schema.isEmpty())
            tooltip << QStringLiteral("Schema: %1").arg(m_entity->schema);
        if (!m_entity->comment.isEmpty())
            tooltip << m_entity->comment;
        setToolTip(tooltip.join(QLatin1Char('\n')));
    }
}

qreal EntityItem::contentWidth() const
{
    return m_width;
}

QRectF EntityItem::boundingRect() const
{
    const qreal height = kHeaderHeight + (m_entity ? m_entity->columns.size() : 0) * kRowHeight;
    // Small margin for the selection border and antialiasing.
    return QRectF(-1, -1, m_width + 2, height + 2);
}

void EntityItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (!m_entity)
        return;

    // Use the scene palette (theme-adaptive).
    const QPalette palette = option ? option->palette : QPalette();
    const bool selected = option && (option->state & QStyle::State_Selected);
    const bool hovered = option && (option->state & QStyle::State_MouseOver);

    const QRectF rect(0, 0, m_width, kHeaderHeight + m_entity->columns.size() * kRowHeight);
    const qreal radius = kCornerRadius;

    painter->setRenderHint(QPainter::Antialiasing);

    // Box with rounded corners.
    QColor borderColor = palette.color(QPalette::Mid);
    if (selected)
        borderColor = palette.color(QPalette::Highlight);
    QColor bodyColor = palette.color(QPalette::Base);
    if (hovered && !selected)
        bodyColor = bodyColor.darker(104);

    QPainterPath boxPath;
    boxPath.addRoundedRect(rect, radius, radius);
    painter->setPen(QPen(borderColor, selected ? 2.0 : 1.0));
    painter->setBrush(bodyColor);
    painter->drawPath(boxPath);

    // Header.
    QColor headerColor = palette.color(QPalette::Highlight);
    if (selected)
        headerColor = headerColor.darker(115);
    else if (hovered)
        headerColor = headerColor.lighter(108);
    painter->fillPath(roundedTopPath(QRectF(0, 0, m_width, kHeaderHeight), radius), headerColor);

    QFont titleFont = painter->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 1.0);
    painter->setFont(titleFont);
    painter->setPen(palette.color(QPalette::HighlightedText));
    painter->drawText(QRectF(0, 0, m_width, kHeaderHeight), Qt::AlignCenter, m_entity->name);

    QFont bodyFont = painter->font();
    bodyFont.setBold(false);
    bodyFont.setPointSizeF(bodyFont.pointSizeF() - 1.0);
    painter->setFont(bodyFont);

    const QFontMetricsF metrics = painter->fontMetrics();
    QColor mutedColor = palette.color(QPalette::WindowText);
    mutedColor.setAlpha(170);

    qreal y = kHeaderHeight;
    for (int i = 0; i < m_entity->columns.size(); ++i) {
        const Column &column = m_entity->columns.at(i);
        const QRectF rowRect(0, y, m_width, kRowHeight);

        if (selected) {
            QColor selectionTint = palette.color(QPalette::Highlight);
            selectionTint.setAlpha(28);
            painter->fillRect(rowRect, selectionTint);
        } else if (i % 2 == 1) {
            painter->fillRect(rowRect, palette.color(QPalette::AlternateBase));
        }

        const qreal chipWidth = kBadgeWidth;
        if (column.primaryKey) {
            // Filled accent chip.
            const QRectF chipRect(kHPadding, y + (kRowHeight - 15.0) / 2.0, chipWidth, 15.0);
            painter->setPen(Qt::NoPen);
            painter->setBrush(palette.color(QPalette::Highlight).lighter(118));
            painter->drawRoundedRect(chipRect, 3.0, 3.0);
            painter->setPen(palette.color(QPalette::HighlightedText));
            painter->drawText(chipRect, Qt::AlignCenter, QStringLiteral("PK"));
        } else if (column.unique) {
            // Outlined chip.
            const QRectF chipRect(kHPadding, y + (kRowHeight - 15.0) / 2.0, chipWidth, 15.0);
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(mutedColor, 1.0));
            painter->drawRoundedRect(chipRect, 3.0, 3.0);
            painter->setPen(mutedColor);
            painter->drawText(chipRect, Qt::AlignCenter, QStringLiteral("U"));
        }

        // Column name.
        const qreal nameX = kHPadding + chipWidth;
        const qreal nameWidth = metrics.horizontalAdvance(column.name);
        painter->setPen(palette.color(QPalette::WindowText));
        painter->drawText(QRectF(nameX, y, nameWidth, kRowHeight), Qt::AlignLeft | Qt::AlignVCenter,
                          column.name);

        // Not-null marker, a small filled dot right after the name.
        if (!column.nullable) {
            const QPointF dotCenter(nameX + nameWidth + 6.0, y + kRowHeight / 2.0);
            painter->setPen(Qt::NoPen);
            painter->setBrush(palette.color(QPalette::WindowText));
            painter->drawEllipse(dotCenter, 2.2, 2.2);
        }

        // Type, right-aligned, muted.
        const QString typeText = column.type.displayName();
        const qreal typeWidth = metrics.horizontalAdvance(typeText);
        const QRectF typeRect(m_width - kHPadding - typeWidth, y, typeWidth, kRowHeight);
        painter->setPen(mutedColor);
        painter->drawText(typeRect, Qt::AlignRight | Qt::AlignVCenter, typeText);

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