#include "DocumentController.h"

namespace bb {

DocumentController::DocumentController(QObject *parent)
    : QObject(parent)
{
    Diagram diagram;
    diagram.id = Id::create();
    diagram.name = QStringLiteral("Main");
    m_model.diagrams.append(diagram);
    m_activeDiagramId = diagram.id;
}

Diagram *DocumentController::activeDiagram()
{
    for (Diagram &diagram : m_model.diagrams) {
        if (diagram.id == m_activeDiagramId)
            return &diagram;
    }
    return nullptr;
}

const Diagram *DocumentController::activeDiagram() const
{
    for (const Diagram &diagram : m_model.diagrams) {
        if (diagram.id == m_activeDiagramId)
            return &diagram;
    }
    return nullptr;
}

Entity *DocumentController::addEntity(const QString &name)
{
    Entity entity;
    entity.id = Id::create();
    entity.name = name;
    m_model.entities.append(entity);

    if (Diagram *diagram = activeDiagram()) {
        const int index = static_cast<int>(m_model.entities.size()) - 1;
        const int column = index % 6;
        const int row = index / 6;
        DiagramNode node;
        node.entity = entity.id;
        node.pos = QPointF(48.0 + column * 260.0, 48.0 + row * 140.0);
        diagram->nodes.append(node);
    }

    emit modelChanged();
    return &m_model.entities.last();
}

} // namespace bb