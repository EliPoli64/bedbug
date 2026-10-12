#include "DocumentController.h"

namespace bb {

namespace {

bool columnsBelongTo(const Model &model, const Id &entityId, const QList<Id> &columnIds)
{
    const Entity *entity = model.findEntity(entityId);
    if (!entity)
        return false;
    for (const Id &columnId : columnIds) {
        if (!entity->findColumn(columnId))
            return false;
    }
    return true;
}

} // namespace

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

Relationship *DocumentController::addRelationship(const Id &childEntityId,
                                                  const QList<Id> &childColumnIds,
                                                  const Id &parentEntityId,
                                                  const QList<Id> &parentColumnIds,
                                                  const QString &name)
{
    if (childEntityId == parentEntityId)
        return nullptr;
    if (childColumnIds.isEmpty() || parentColumnIds.isEmpty())
        return nullptr;
    if (!columnsBelongTo(m_model, childEntityId, childColumnIds))
        return nullptr;
    if (!columnsBelongTo(m_model, parentEntityId, parentColumnIds))
        return nullptr;

    Relationship relationship;
    relationship.id = Id::create();
    relationship.childEntity = childEntityId;
    relationship.childColumns = childColumnIds;
    relationship.parentEntity = parentEntityId;
    relationship.parentColumns = parentColumnIds;

    if (name.isEmpty()) {
        const Entity *child = m_model.findEntity(childEntityId);
        const Entity *parent = m_model.findEntity(parentEntityId);
        relationship.name = QStringLiteral("fk_%1_%2")
                                .arg(child ? child->name : QStringLiteral("child"),
                                     parent ? parent->name : QStringLiteral("parent"));
    } else {
        relationship.name = name;
    }

    m_model.relationships.append(relationship);
    emit modelChanged();
    return &m_model.relationships.last();
}

bool DocumentController::removeEntity(const Id &entityId)
{
    int index = -1;
    for (int i = 0; i < m_model.entities.size(); ++i) {
        if (m_model.entities.at(i).id == entityId) {
            index = i;
            break;
        }
    }
    if (index < 0)
        return false;

    // Drop every relationship that touches this entity.
    m_model.relationships.removeIf([entityId](const Relationship &relationship) {
        return relationship.childEntity == entityId || relationship.parentEntity == entityId;
    });

    // Drop every diagram node that references this entity.
    for (Diagram &diagram : m_model.diagrams) {
        diagram.nodes.removeIf(
            [entityId](const DiagramNode &node) { return node.entity == entityId; });
    }

    m_model.entities.removeAt(index);
    emit modelChanged();
    return true;
}

bool DocumentController::removeColumn(const Id &entityId, const Id &columnId)
{
    Entity *entity = m_model.findEntity(entityId);
    if (!entity)
        return false;

    int index = -1;
    for (int i = 0; i < entity->columns.size(); ++i) {
        if (entity->columns.at(i).id == columnId) {
            index = i;
            break;
        }
    }
    if (index < 0)
        return false;

    // Drop relationships that reference the removed column.
    m_model.relationships.removeIf([columnId](const Relationship &relationship) {
        return relationship.childColumns.contains(columnId)
               || relationship.parentColumns.contains(columnId);
    });

    // Drop indexes that referenced the column.
    entity->indexes.removeIf(
        [columnId](const Index &index) { return index.columnIds.contains(columnId); });

    entity->columns.removeAt(index);
    emit modelChanged();
    return true;
}

bool DocumentController::renameEntity(const Id &entityId, const QString &name)
{
    if (name.trimmed().isEmpty())
        return false;
    Entity *entity = m_model.findEntity(entityId);
    if (!entity)
        return false;
    entity->name = name;
    emit modelChanged();
    return true;
}

Column *DocumentController::addColumn(const Id &entityId, const QString &name,
                                      const CanonicalType &type, bool nullable, bool primaryKey,
                                      bool unique, bool autoIncrement, const QString &defaultValue,
                                      const QString &comment)
{
    Entity *entity = m_model.findEntity(entityId);
    if (!entity)
        return nullptr;

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return nullptr;

    // Column names must be unique within a table (compared case-insensitively, as most
    // engines resolve unquoted identifiers).
    for (const Column &column : entity->columns) {
        if (column.name.compare(trimmed, Qt::CaseInsensitive) == 0)
            return nullptr;
    }

    Column column;
    column.id = Id::create();
    column.name = trimmed;
    column.type = type;
    column.nullable = nullable;
    column.primaryKey = primaryKey;
    column.unique = unique;
    column.autoIncrement = autoIncrement;
    column.defaultValue = defaultValue;
    column.comment = comment;

    entity->columns.append(column);
    emit modelChanged();
    return &entity->columns.last();
}

bool DocumentController::updateColumn(const Id &entityId, const Column &column)
{
    Entity *entity = m_model.findEntity(entityId);
    if (!entity)
        return false;

    const QString trimmed = column.name.trimmed();
    if (trimmed.isEmpty())
        return false;

    // Renames must not collide with a sibling column.
    for (const Column &other : entity->columns) {
        if (other.id == column.id)
            continue;
        if (other.name.compare(trimmed, Qt::CaseInsensitive) == 0)
            return false;
    }

    for (Column &candidate : entity->columns) {
        if (candidate.id == column.id) {
            Column updated = column;
            updated.name = trimmed;
            updated.id = column.id;   // ids are immutable
            candidate = updated;
            emit modelChanged();
            return true;
        }
    }
    return false;
}

} // namespace bb