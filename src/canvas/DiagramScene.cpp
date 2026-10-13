#include "DiagramScene.h"

#include "RelationshipItem.h"

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>

#include <utility>

namespace bb {

namespace {

DiagramNode *nodeFor(Diagram *diagram, const Id &entityId)
{
    if (!diagram)
        return nullptr;
    for (DiagramNode &node : diagram->nodes) {
        if (node.entity == entityId)
            return &node;
    }
    return nullptr;
}

} // namespace

DiagramScene::DiagramScene(QObject *parent)
    : QGraphicsScene(parent)
{
}

void DiagramScene::setModel(Model *model)
{
    m_model = model;
}

void DiagramScene::setActiveDiagramId(const Id &diagramId)
{
    m_diagramId = diagramId;
}

Diagram *DiagramScene::findDiagram()
{
    if (!m_model)
        return nullptr;
    for (Diagram &diagram : m_model->diagrams) {
        if (diagram.id == m_diagramId)
            return &diagram;
    }
    return nullptr;
}

Diagram *DiagramScene::currentDiagram()
{
    return findDiagram();
}

EntityItem *DiagramScene::itemForEntity(const Id &entityId) const
{
    return m_items.value(entityId, nullptr);
}

RelationshipItem *DiagramScene::itemForRelationship(const Id &relationshipId) const
{
    return m_relationshipItems.value(relationshipId, nullptr);
}

void DiagramScene::setLinkTool(std::optional<RelationshipKind> tool)
{
    if (m_linkTool == tool)
        return;
    m_linkTool = tool;
    m_linkSourceEntity = Id();
    m_linkSourceColumn = Id();

    if (m_linkTool.has_value()) {
        const char *label = nullptr;
        switch (*m_linkTool) {
        case RelationshipKind::OneToOne:
            label = "1:1";
            break;
        case RelationshipKind::OneToMany:
            label = "1:N";
            break;
        case RelationshipKind::ManyToMany:
            label = "N:M";
            break;
        }
        emit linkStatusChanged(tr("%1 relationship tool: click a source column, then a target "
                                  "column on another table")
                                   .arg(QLatin1String(label)));
    } else {
        m_linkSourceEntity = Id();
        m_linkSourceColumn = Id();
        emit linkStatusChanged(QString());
    }
}

void DiagramScene::setRelationshipCreationHandler(
    std::function<bool(const Id &, const QList<Id> &, const Id &, const QList<Id> &,
                       RelationshipKind)>
        handler)
{
    m_relationshipHandler = std::move(handler);
}

void DiagramScene::beginOrCompleteLink(const Id &entityId, const Id &columnId)
{
    if (!m_linkTool.has_value())
        return;

    if (m_linkSourceEntity.isNull()) {
        m_linkSourceEntity = entityId;
        m_linkSourceColumn = columnId;
        QString label;
        if (m_model) {
            if (const Entity *entity = m_model->findEntity(entityId))
                if (!columnId.isNull())
                    if (const Column *column = entity->findColumn(columnId))
                        label = QStringLiteral("%1.%2").arg(entity->name, column->name);
        }
        emit linkStatusChanged(
            tr("Source %1 — now click a target column on another table").arg(label));
        return;
    }

    if (m_linkSourceEntity == entityId) {
        // Same table: restart the source at this point instead.
        m_linkSourceEntity = entityId;
        m_linkSourceColumn = columnId;
        emit linkStatusChanged(tr("Restarted source on this table"));
        return;
    }

    const Id childEntity = m_linkSourceEntity;
    const Id childColumn = m_linkSourceColumn;
    m_linkSourceEntity = Id();
    m_linkSourceColumn = Id();

    if (m_relationshipHandler) {
        const QList<Id> childColumns =
            childColumn.isNull() ? QList<Id>() : QList<Id>{childColumn};
        const QList<Id> parentColumns = columnId.isNull() ? QList<Id>() : QList<Id>{columnId};
        const bool created =
            m_relationshipHandler(childEntity, childColumns, entityId, parentColumns, *m_linkTool);
        emit linkStatusChanged(created ? tr("Relationship created")
                                       : tr("Could not create relationship"));
    }
}

bool DiagramScene::handleLinkAt(const QPointF &scenePos)
{
    if (!m_linkTool.has_value())
        return false;

    EntityItem *entityItem = nullptr;
    const QList<QGraphicsItem *> hits = items(scenePos);
    for (QGraphicsItem *item : hits) {
        if (auto *entity = dynamic_cast<EntityItem *>(item)) {
            entityItem = entity;
            break;
        }
    }

    if (!entityItem) {
        m_linkSourceEntity = Id();
        m_linkSourceColumn = Id();
        emit linkStatusChanged(tr("Click a column"));
        return true;
    }

    const int row = entityItem->columnAt(entityItem->mapFromScene(scenePos));
    if (row < 0) {
        // Tapping the table (header/body) auto-resolves the column on completion.
        beginOrCompleteLink(entityItem->entityId(), Id());
        return true;
    }

    const Column &column = entityItem->entity()->columns.at(row);
    beginOrCompleteLink(entityItem->entityId(), column.id);
    return true;
}

bool DiagramScene::handleTableContextMenu(const QPointF &scenePos, const QPoint &globalPos)
{
    EntityItem *entityItem = nullptr;
    const QList<QGraphicsItem *> hits = items(scenePos);
    for (QGraphicsItem *item : hits) {
        if (auto *entity = dynamic_cast<EntityItem *>(item)) {
            entityItem = entity;
            break;
        }
    }
    if (!entityItem)
        return false;

    emit tableContextMenuRequested(entityItem->entityId(), globalPos);
    return true;
}

void DiagramScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (handleTableContextMenu(event->scenePos(), event->screenPos())) {
        event->accept();
        return;
    }
    QGraphicsScene::contextMenuEvent(event);
}

void DiagramScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_linkTool.has_value() && event->button() == Qt::LeftButton) {
        handleLinkAt(event->scenePos());
        event->accept();
        return;
    }
    QGraphicsScene::mousePressEvent(event);
}

void DiagramScene::refreshRelationships()
{
    for (RelationshipItem *item : std::as_const(m_relationshipItems))
        item->refresh();
}

void DiagramScene::syncFromModel()
{
    clear();
    m_items.clear();
    m_relationshipItems.clear();
    m_linkSourceEntity = Id();
    m_linkSourceColumn = Id();

    if (!m_model)
        return;

    Diagram *diagram = findDiagram();
    if (!diagram)
        return;

    for (const DiagramNode &node : std::as_const(diagram->nodes)) {
        const Entity *entity = m_model->findEntity(node.entity);
        if (!entity)
            continue;

        auto *item = new EntityItem(entity);
        item->setFlag(QGraphicsItem::ItemIsMovable);
        item->setFlag(QGraphicsItem::ItemIsSelectable);
        item->setFlag(QGraphicsItem::ItemSendsGeometryChanges);
        item->setPos(node.pos);

        const Id entityId = entity->id;
        item->setOnMoved([this, entityId](const QPointF &pos) {
            if (DiagramNode *node = nodeFor(findDiagram(), entityId))
                node->pos = pos;
            refreshRelationships();
            emit entityMoved(entityId, pos);
        });

        m_items.insert(entityId, item);
        addItem(item);
    }

    for (const Relationship &relationship : std::as_const(m_model->relationships)) {
        // Only show edges whose endpoints are both present in this diagram.
        if (!m_items.contains(relationship.childEntity) || !m_items.contains(relationship.parentEntity))
            continue;

        auto *item = new RelationshipItem(relationship.id, this);
        item->setZValue(-1.0);
        item->setFlag(QGraphicsItem::ItemIsSelectable);
        item->refresh();
        m_relationshipItems.insert(relationship.id, item);
        addItem(item);
    }
}

} // namespace bb