#include "DiagramScene.h"

#include "RelationshipItem.h"

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

void DiagramScene::setLinkMode(bool enabled)
{
    if (m_linkMode == enabled)
        return;
    m_linkMode = enabled;
    m_linkSourceEntity = Id();
    m_linkSourceColumn = Id();
    emit linkStatusChanged(enabled ? tr("Link mode: click a source column, then a target column")
                                   : QString());
}

void DiagramScene::setRelationshipCreationHandler(
    std::function<void(const Id &, const QList<Id> &, const Id &, const QList<Id> &)> handler)
{
    m_relationshipHandler = std::move(handler);
}

void DiagramScene::beginOrCompleteLink(const Id &entityId, const Id &columnId)
{
    if (m_linkSourceEntity.isNull()) {
        m_linkSourceEntity = entityId;
        m_linkSourceColumn = columnId;
        QString label;
        if (m_model) {
            if (const Entity *entity = m_model->findEntity(entityId))
                if (const Column *column = entity->findColumn(columnId))
                    label = QStringLiteral("%1.%2").arg(entity->name, column->name);
        }
        emit linkStatusChanged(tr("Source %1 — now click a target column on another table")
                                   .arg(label));
        return;
    }

    if (m_linkSourceEntity == entityId) {
        // Same table: restart the source at this column instead.
        m_linkSourceEntity = entityId;
        m_linkSourceColumn = columnId;
        emit linkStatusChanged(tr("Restarted source on this column"));
        return;
    }

    const Id childEntity = m_linkSourceEntity;
    const Id childColumn = m_linkSourceColumn;
    m_linkSourceEntity = Id();
    m_linkSourceColumn = Id();

    if (m_relationshipHandler) {
        m_relationshipHandler(childEntity, {childColumn}, entityId, {columnId});
        emit linkStatusChanged(tr("Relationship created"));
    }
}

bool DiagramScene::handleLinkAt(const QPointF &scenePos)
{
    if (!m_linkMode)
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
    if (row < 0)
        return true;

    const Column &column = entityItem->entity()->columns.at(row);
    beginOrCompleteLink(entityItem->entityId(), column.id);
    return true;
}

void DiagramScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_linkMode && event->button() == Qt::LeftButton) {
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