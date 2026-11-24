#include "DocumentController.h"

#include "JsonSerializer.h"
#include "commands/MoveEntityNodeCommand.h"
#include "commands/ModelSnapshotCommand.h"

#include <QFile>
#include <QHashFunctions>
#include <QJsonDocument>
#include <QRectF>
#include <QSettings>
#include <QUndoStack>

namespace bb {

namespace {

bool loadJsonDocument(const QString &path, Model *model, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("File does not contain a valid bedbug document");
        return false;
    }
    QString error;
    *model = JsonSerializer::fromJson(doc.object(), &error);
    if (!error.isEmpty()) {
        if (errorMessage)
            *errorMessage = error;
        return false;
    }
    return true;
}

bool writeModelTo(const Model &model, const QString &path, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    file.write(JsonSerializer::toBytes(model, true));
    file.close();
    if (file.error() != QFileDevice::NoError) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Write failed: %1").arg(file.errorString());
        return false;
    }
    return true;
}

void ensureDiagram(Model *model, Id *activeDiagramId)
{
    if (model->diagrams.isEmpty()) {
        Diagram diagram;
        diagram.id = Id::create();
        diagram.name = QStringLiteral("Main");
        model->diagrams.append(diagram);
    }
    *activeDiagramId = model->diagrams.first().id;
}

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

    connect(&m_undoStack, &QUndoStack::cleanChanged, this, [this](bool) {
        refreshModified();
    });
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

Entity *DocumentController::addEntity(const QString &name, std::optional<QPointF> position)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return nullptr;

    // Table names must be unique (case-insensitive, matching typical engines).
    for (const Entity &entity : m_model.entities) {
        if (entity.name.compare(trimmed, Qt::CaseInsensitive) == 0)
            return nullptr;
    }

    const QJsonObject before = JsonSerializer::toJson(m_model);

    Entity entity;
    entity.id = Id::create();
    entity.name = trimmed;

    // Spawn with a primary key by default. The name format is configurable via
    // QSettings ("defaultPkFormat", default "{table}ID", {table} lowercased) and the
    // whole behaviour can be turned off in Settings → Deactivation.
    if (QSettings().value(QStringLiteral("autoAddPrimaryKey"), true).toBool()) {
        const QString format = QSettings().value(QStringLiteral("defaultPkFormat"),
                                                 QStringLiteral("{table}ID")).toString();
        QString pkName = format.isEmpty() ? QStringLiteral("{table}ID") : format;
        pkName.replace(QStringLiteral("{table}"), trimmed.toLower());

        Column primaryKey;
        primaryKey.id = Id::create();
        primaryKey.name = pkName;
        primaryKey.type.kind = CanonicalType::Kind::BigInt;
        primaryKey.nullable = false;
        primaryKey.primaryKey = true;
        primaryKey.unique = true;
        primaryKey.autoIncrement = true;
        entity.columns.append(primaryKey);
    }

    m_model.entities.append(entity);

    if (Diagram *diagram = activeDiagram()) {
        DiagramNode node;
        node.entity = entity.id;
        if (position) {
            node.pos = *position;
        } else {
            const int index = static_cast<int>(m_model.entities.size()) - 1;
            const int column = index % 6;
            const int row = index / 6;
            node.pos = QPointF(48.0 + column * 260.0, 48.0 + row * 140.0);
        }
        diagram->nodes.append(node);
    }

    commit(before, tr("Add table"));
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

    const QJsonObject before = JsonSerializer::toJson(m_model);

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
    commit(before, tr("Create relationship"));
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

    const QJsonObject before = JsonSerializer::toJson(m_model);

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
    commit(before, tr("Remove table"));
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

    const QJsonObject before = JsonSerializer::toJson(m_model);

    // Drop relationships that reference the removed column.
    m_model.relationships.removeIf([columnId](const Relationship &relationship) {
        return relationship.childColumns.contains(columnId)
               || relationship.parentColumns.contains(columnId);
    });

    // Drop indexes that referenced the column.
    entity->indexes.removeIf(
        [columnId](const Index &index) { return index.columnIds.contains(columnId); });

    entity->columns.removeAt(index);
    commit(before, tr("Remove column"));
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

    const QJsonObject before = JsonSerializer::toJson(m_model);
    entity->name = trimmed;
    commit(before, tr("Rename table"));
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

    const QJsonObject before = JsonSerializer::toJson(m_model);

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
    commit(before, tr("Add column"));
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

    const QJsonObject before = JsonSerializer::toJson(m_model);
    for (Column &candidate : entity->columns) {
        if (candidate.id == column.id) {
            Column updated = column;
            updated.name = trimmed;
            updated.id = column.id;   // ids are immutable
            candidate = updated;
            commit(before, tr("Edit column"));
            return true;
        }
    }
    return false;
}

int DocumentController::applyTypeOverrides(const QString &dialectId,
                                           const QList<ColumnTypeOverride> &overrides)
{
    if (dialectId.isEmpty() || overrides.isEmpty())
        return 0;

    QList<ColumnTypeOverride> applicable;
    for (const ColumnTypeOverride &override : overrides) {
        Entity *entity = m_model.findEntity(override.entityId);
        if (!entity)
            continue;
        Column *column = entity->findColumn(override.columnId);
        if (!column)
            continue;
        if (column->dialectOverrides.value(dialectId) == override.rawType)
            continue;
        applicable.append(override);
    }
    if (applicable.isEmpty())
        return 0;

    const QJsonObject before = JsonSerializer::toJson(m_model);
    for (const ColumnTypeOverride &override : applicable) {
        Entity *entity = m_model.findEntity(override.entityId);
        Column *column = entity ? entity->findColumn(override.columnId) : nullptr;
        if (column)
            column->dialectOverrides.insert(dialectId, override.rawType);
    }

    commit(before, tr("Resolve types"));
    return applicable.size();
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

    const QJsonObject before = JsonSerializer::toJson(m_model);

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

    commit(before, tr("Create junction table"));
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

    const QJsonObject before = JsonSerializer::toJson(m_model);

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
    commit(before, tr("Create relationship"));
    return &m_model.relationships.last();
}

// --- File lifecycle -------------------------------------------------------

void DocumentController::commit(const QJsonObject &before, const QString &text)
{
    m_undoStack.push(
        new ModelSnapshotCommand(this, before, JsonSerializer::toJson(m_model), text));
    refreshModified();
    emit modelChanged();
}

void DocumentController::applySnapshot(const QJsonObject &snapshot)
{
    m_model = JsonSerializer::fromJson(snapshot);
    emit modelChanged();
    refreshModified();
}

void DocumentController::refreshModified()
{
    const bool modified = m_externalDirty || !m_undoStack.isClean();
    if (m_modified == modified)
        return;
    m_modified = modified;
    emit modifiedChanged(m_modified);
}

void DocumentController::resetHistory()
{
    m_externalDirty = false;
    m_undoStack.clear();
    refreshModified();
}

void DocumentController::moveEntityNode(const Id &entityId, const QPointF &position)
{
    Diagram *diagram = activeDiagram();
    if (!diagram)
        return;

    DiagramNode *node = nullptr;
    for (DiagramNode &candidate : diagram->nodes) {
        if (candidate.entity == entityId) {
            node = &candidate;
            break;
        }
    }
    if (!node || node->pos == position)
        return;

    const QPointF before = node->pos;
    node->pos = position;

    // One stable merge key per (diagram, table): consecutive pixel moves collapse into
    // a single undo step.
    const int mergeId =
        static_cast<int>(qHash(diagram->id, qHash(entityId, 0)) & 0x7fffffffu);
    m_undoStack.push(
        new MoveEntityNodeCommand(this, diagram->id, entityId, before, position, mergeId));
    refreshModified();
}

void DocumentController::applyNodePosition(const Id &diagramId, const Id &entityId,
                                           const QPointF &position)
{
    for (Diagram &diagram : m_model.diagrams) {
        if (diagram.id != diagramId)
            continue;
        for (DiagramNode &node : diagram.nodes) {
            if (node.entity != entityId)
                continue;
            node.pos = position;
            emit modelChanged();
            refreshModified();
            return;
        }
    }
}

bool DocumentController::loadFromFile(const QString &path, QString *errorMessage)
{
    Model loaded;
    if (!loadJsonDocument(path, &loaded, errorMessage))
        return false;

    Id activeId;
    ensureDiagram(&loaded, &activeId);
    m_model = loaded;
    m_activeDiagramId = activeId;
    m_filePath = path;
    resetHistory();
    emit modelChanged();
    return true;
}

bool DocumentController::saveToFile(const QString &path, QString *errorMessage)
{
    if (!writeModelTo(m_model, path, errorMessage))
        return false;
    m_filePath = path;
    m_externalDirty = false;
    m_undoStack.setClean();
    refreshModified();
    return true;
}

bool DocumentController::loadSession(const QString &path, QString *errorMessage)
{
    Model loaded;
    if (!loadJsonDocument(path, &loaded, errorMessage))
        return false;

    Id activeId;
    ensureDiagram(&loaded, &activeId);
    m_model = loaded;
    m_activeDiagramId = activeId;
    resetHistory();
    emit modelChanged();
    return true;
}

bool DocumentController::saveSession(const QString &path, QString *errorMessage) const
{
    return writeModelTo(m_model, path, errorMessage);
}

void DocumentController::newDocument()
{
    m_model = Model();
    Diagram diagram;
    diagram.id = Id::create();
    diagram.name = QStringLiteral("Main");
    m_model.diagrams.append(diagram);
    m_activeDiagramId = diagram.id;
    m_filePath.clear();
    resetHistory();
    emit modelChanged();
}

void DocumentController::setCanvasSize(int pagesX, int pagesY)
{
    if (Diagram *diagram = activeDiagram()) {
        const QJsonObject before = JsonSerializer::toJson(m_model);
        diagram->pagesX = qMax(1, pagesX);
        diagram->pagesY = qMax(1, pagesY);
        diagram->originX = 0;
        diagram->originY = 0;
        commit(before, tr("Resize canvas"));
    }
}

namespace {

bool sheetHasNodes(const Diagram &diagram, int sheetX, int sheetY, qreal sheetWidth,
                   qreal sheetHeight)
{
    const qreal left = sheetX * sheetWidth;
    const qreal top = sheetY * sheetHeight;
    const QRectF sheetRect(left, top, sheetWidth, sheetHeight);
    for (const DiagramNode &node : diagram.nodes) {
        if (sheetRect.contains(node.pos))
            return true;
    }
    return false;
}

} // namespace

void DocumentController::addSheetLeft()
{
    if (Diagram *diagram = activeDiagram()) {
        const QJsonObject before = JsonSerializer::toJson(m_model);
        diagram->originX -= 1;
        diagram->pagesX += 1;
        commit(before, tr("Add sheet"));
    }
}

void DocumentController::addSheetRight()
{
    if (Diagram *diagram = activeDiagram()) {
        const QJsonObject before = JsonSerializer::toJson(m_model);
        diagram->pagesX += 1;
        commit(before, tr("Add sheet"));
    }
}

void DocumentController::addSheetUp()
{
    if (Diagram *diagram = activeDiagram()) {
        const QJsonObject before = JsonSerializer::toJson(m_model);
        diagram->originY -= 1;
        diagram->pagesY += 1;
        commit(before, tr("Add sheet"));
    }
}

void DocumentController::addSheetDown()
{
    if (Diagram *diagram = activeDiagram()) {
        const QJsonObject before = JsonSerializer::toJson(m_model);
        diagram->pagesY += 1;
        commit(before, tr("Add sheet"));
    }
}

void DocumentController::removeSheetLeft()
{
    if (Diagram *diagram = activeDiagram()) {
        if (diagram->pagesX <= 1)
            return;
        // Block removal when the leftmost column still holds content.
        for (int y = 0; y < diagram->pagesY; ++y) {
            if (sheetHasNodes(*diagram, diagram->originX, diagram->originY + y,
                              Diagram::kSheetWidth, Diagram::kSheetHeight))
                return;
        }
        const QJsonObject before = JsonSerializer::toJson(m_model);
        diagram->originX += 1;
        diagram->pagesX -= 1;
        commit(before, tr("Remove sheet"));
    }
}

void DocumentController::removeSheetRight()
{
    if (Diagram *diagram = activeDiagram()) {
        if (diagram->pagesX <= 1)
            return;
        const int lastSheetX = diagram->originX + diagram->pagesX - 1;
        for (int y = 0; y < diagram->pagesY; ++y) {
            if (sheetHasNodes(*diagram, lastSheetX, diagram->originY + y,
                              Diagram::kSheetWidth, Diagram::kSheetHeight))
                return;
        }
        const QJsonObject before = JsonSerializer::toJson(m_model);
        diagram->pagesX -= 1;
        commit(before, tr("Remove sheet"));
    }
}

void DocumentController::removeSheetUp()
{
    if (Diagram *diagram = activeDiagram()) {
        if (diagram->pagesY <= 1)
            return;
        for (int x = 0; x < diagram->pagesX; ++x) {
            if (sheetHasNodes(*diagram, diagram->originX + x, diagram->originY,
                              Diagram::kSheetWidth, Diagram::kSheetHeight))
                return;
        }
        const QJsonObject before = JsonSerializer::toJson(m_model);
        diagram->originY += 1;
        diagram->pagesY -= 1;
        commit(before, tr("Remove sheet"));
    }
}

void DocumentController::removeSheetDown()
{
    if (Diagram *diagram = activeDiagram()) {
        if (diagram->pagesY <= 1)
            return;
        const int lastSheetY = diagram->originY + diagram->pagesY - 1;
        for (int x = 0; x < diagram->pagesX; ++x) {
            if (sheetHasNodes(*diagram, diagram->originX + x, lastSheetY,
                              Diagram::kSheetWidth, Diagram::kSheetHeight))
                return;
        }
        const QJsonObject before = JsonSerializer::toJson(m_model);
        diagram->pagesY -= 1;
        commit(before, tr("Remove sheet"));
    }
}

void DocumentController::markDirty()
{
    if (m_externalDirty)
        return;
    m_externalDirty = true;
    refreshModified();
}

} // namespace bb