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

QString sanitizeTableName(const QString &name)
{
    QString out;
    for (const QChar &c : name.toLower()) {
        const bool alnum = (c >= QLatin1Char('a') && c <= QLatin1Char('z'))
                           || (c >= QLatin1Char('0') && c <= QLatin1Char('9'));
        out += alnum ? c : QLatin1Char('_');
    }
    while (out.startsWith(QLatin1Char('_')))
        out.remove(0, 1);
    while (out.endsWith(QLatin1Char('_')))
        out.chop(1);
    return out;
}

const Column *primaryColumnOrFirst(const Entity &entity)
{
    for (const Column &column : entity.columns) {
        if (column.primaryKey)
            return &column;
    }
    return entity.columns.isEmpty() ? nullptr : &entity.columns.first();
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
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return nullptr;

    // Table names must be unique (case-insensitive, matching typical engines).
    for (const Entity &entity : m_model.entities) {
        if (entity.name.compare(trimmed, Qt::CaseInsensitive) == 0)
            return nullptr;
    }

    Entity entity;
    entity.id = Id::create();
    entity.name = trimmed;
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
    return addRelationship(RelationshipKind::OneToMany, childEntityId, childColumnIds,
                           parentEntityId, parentColumnIds, name);
}

Relationship *DocumentController::addRelationship(RelationshipKind kind, const Id &childEntityId,
                                                  const QList<Id> &childColumnIds,
                                                  const Id &parentEntityId,
                                                  const QList<Id> &parentColumnIds,
                                                  const QString &name)
{
    if (kind == RelationshipKind::ManyToMany)
        return nullptr;
    if (childEntityId == parentEntityId)
        return nullptr;
    if (childColumnIds.isEmpty() || parentColumnIds.isEmpty())
        return nullptr;
    if (!columnsBelongTo(m_model, childEntityId, childColumnIds))
        return nullptr;
    if (!columnsBelongTo(m_model, parentEntityId, parentColumnIds))
        return nullptr;

    // 1:1 relationships are enforced by a unique constraint on the child FK column(s).
    if (kind == RelationshipKind::OneToOne) {
        if (Entity *child = m_model.findEntity(childEntityId)) {
            for (const Id &columnId : childColumnIds) {
                if (Column *column = child->findColumn(columnId))
                    column->unique = true;
            }
        }
    }

    Relationship relationship;
    relationship.id = Id::create();
    relationship.childEntity = childEntityId;
    relationship.childColumns = childColumnIds;
    relationship.parentEntity = parentEntityId;
    relationship.parentColumns = parentColumnIds;
    relationship.identifying = (kind == RelationshipKind::OneToOne);

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
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return false;
    Entity *entity = m_model.findEntity(entityId);
    if (!entity)
        return false;

    // Renames must not collide with a sibling table.
    for (const Entity &other : m_model.entities) {
        if (other.id == entityId)
            continue;
        if (other.name.compare(trimmed, Qt::CaseInsensitive) == 0)
            return false;
    }

    entity->name = trimmed;
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

Entity *DocumentController::createManyToMany(const Id &leftEntityId, const Id &rightEntityId)
{
    Entity *left = m_model.findEntity(leftEntityId);
    Entity *right = m_model.findEntity(rightEntityId);
    if (!left || !right || leftEntityId == rightEntityId)
        return nullptr;

    const Column *leftRef = primaryColumnOrFirst(*left);
    const Column *rightRef = primaryColumnOrFirst(*right);
    if (!leftRef || !rightRef)
        return nullptr;

    const QString leftName = sanitizeTableName(left->name);
    const QString rightName = sanitizeTableName(right->name);
    if (leftName.isEmpty() || rightName.isEmpty())
        return nullptr;

    // Junction table: composite PK made of the two FKs.
    Entity junction;
    junction.id = Id::create();
    junction.name = leftName + QLatin1Char('_') + rightName;
    junction.schema = left->schema;

    Column leftColumn;
    leftColumn.id = Id::create();
    leftColumn.name = leftName + QStringLiteral("_id");
    leftColumn.type = leftRef->type;
    leftColumn.nullable = false;
    leftColumn.primaryKey = true;

    Column rightColumn;
    rightColumn.id = Id::create();
    rightColumn.name = rightName + QStringLiteral("_id");
    rightColumn.type = rightRef->type;
    rightColumn.nullable = false;
    rightColumn.primaryKey = true;

    junction.columns = {leftColumn, rightColumn};
    m_model.entities.append(junction);

    Relationship leftFk;
    leftFk.id = Id::create();
    leftFk.name = QStringLiteral("fk_%1_%2").arg(junction.name, leftName);
    leftFk.identifying = true;
    leftFk.childEntity = junction.id;
    leftFk.childColumns = {leftColumn.id};
    leftFk.parentEntity = leftEntityId;
    leftFk.parentColumns = {leftRef->id};
    m_model.relationships.append(leftFk);

    Relationship rightFk;
    rightFk.id = Id::create();
    rightFk.name = QStringLiteral("fk_%1_%2").arg(junction.name, rightName);
    rightFk.identifying = true;
    rightFk.childEntity = junction.id;
    rightFk.childColumns = {rightColumn.id};
    rightFk.parentEntity = rightEntityId;
    rightFk.parentColumns = {rightRef->id};
    m_model.relationships.append(rightFk);

    // Place the junction between its two parents.
    if (Diagram *diagram = activeDiagram()) {
        QPointF leftPos;
        QPointF rightPos;
        for (const DiagramNode &node : diagram->nodes) {
            if (node.entity == leftEntityId)
                leftPos = node.pos;
            else if (node.entity == rightEntityId)
                rightPos = node.pos;
        }
        if (leftPos.isNull() && rightPos.isNull()) {
            leftPos = QPointF(200, 200);
            rightPos = QPointF(200, 200);
        }
        DiagramNode node;
        node.entity = junction.id;
        node.pos = (leftPos + rightPos) / 2.0 + QPointF(24, 24);
        diagram->nodes.append(node);
    }

    emit modelChanged();
    return &m_model.entities.last();
}

Relationship *DocumentController::addRelationshipPick(RelationshipKind kind,
                                                      const Id &childEntityId,
                                                      const Id &childColumnId,
                                                      const Id &parentEntityId,
                                                      const Id &parentColumnId,
                                                      const QString &name)
{
    if (kind == RelationshipKind::ManyToMany)
        return nullptr;
    Entity *child = m_model.findEntity(childEntityId);
    Entity *parent = m_model.findEntity(parentEntityId);
    if (!child || !parent || childEntityId == parentEntityId)
        return nullptr;

    // Parent (referenced) column: explicit > primary key > first column.
    const Column *parentColumn = nullptr;
    if (!parentColumnId.isNull())
        parentColumn = parent->findColumn(parentColumnId);
    if (!parentColumn)
        parentColumn = primaryColumnOrFirst(*parent);
    if (!parentColumn)
        return nullptr;

    // Child (FK) column: explicit > an existing "{parent}_id" > a freshly created one.
    Id childFkId = childColumnId;
    if (childFkId.isNull()) {
        const QString candidate = sanitizeTableName(parent->name) + QStringLiteral("_id");
        for (const Column &column : child->columns) {
            if (column.name.compare(candidate, Qt::CaseInsensitive) == 0) {
                childFkId = column.id;
                break;
            }
        }
    }
    if (childFkId.isNull()) {
        Column column;
        column.id = Id::create();
        column.name = sanitizeTableName(parent->name) + QStringLiteral("_id");
        column.type = parentColumn->type;
        column.nullable = (kind != RelationshipKind::OneToOne);
        column.primaryKey = false;
        if (kind == RelationshipKind::OneToOne)
            column.unique = true;
        child->columns.append(column);
        childFkId = column.id;
    }
    if (!child->findColumn(childFkId))
        return nullptr;

    Relationship relationship;
    relationship.id = Id::create();
    relationship.childEntity = childEntityId;
    relationship.childColumns = {childFkId};
    relationship.parentEntity = parentEntityId;
    relationship.parentColumns = {parentColumn->id};
    relationship.identifying = (kind == RelationshipKind::OneToOne);
    relationship.name = name.isEmpty() ? QStringLiteral("fk_%1_%2")
                                                .arg(child->name, parent->name)
                                       : name;

    // 1:1 relationships are enforced by a unique constraint on the child FK column(s).
    if (kind == RelationshipKind::OneToOne) {
        for (const Id &columnId : relationship.childColumns) {
            if (Column *column = child->findColumn(columnId))
                column->unique = true;
        }
    }

    m_model.relationships.append(relationship);
    emit modelChanged();
    return &m_model.relationships.last();
}

} // namespace bb