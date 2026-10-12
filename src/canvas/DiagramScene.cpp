#include "DiagramScene.h"

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

void DiagramScene::syncFromModel()
{
    clear();
    m_items.clear();

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
            emit entityMoved(entityId, pos);
        });

        m_items.insert(entityId, item);
        addItem(item);
    }
}

} // namespace bb