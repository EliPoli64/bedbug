#include "SqlGenerator.h"

#include <QHash>
#include <QQueue>
#include <QSet>

namespace bb {

namespace {

// "schema"."table" when the entity has a schema and option is on, else "table".
QString qualifiedName(const Entity &entity, const SqlDialect &dialect,
                      const SqlGenOptions &options)
{
    const QString quotedTable = dialect.quoteIdent(entity.name);
    if (options.qualifySchema && !entity.schema.isEmpty())
        return dialect.quoteIdent(entity.schema) + QStringLiteral(".") + quotedTable;
    return quotedTable;
}

QString indexNameFor(const Entity &entity, const Index &index, const SqlDialect &dialect)
{
    if (!index.name.isEmpty())
        return index.name;

    QString derived = QStringLiteral("ix_") + entity.name;
    if (!index.columnIds.isEmpty()) {
        const Column *first = entity.findColumn(index.columnIds.first());
        if (first)
            derived += QStringLiteral("_") + first->name;
    }
    return derived;
}

} // namespace

QList<Id> SqlGenerator::createOrder(const Model &model)
{
    QList<Id> order;
    QHash<Id, QList<Id>> childrenOf;   // parent -> child entity ids
    QHash<Id, int> indegree;

    // A parent must precede its children. Self-relationships are ignored (the table
    // creates itself).
    for (const Relationship &relationship : model.relationships) {
        if (relationship.parentEntity == relationship.childEntity)
            continue;
        const bool parentKnown = model.findEntity(relationship.parentEntity) != nullptr;
        const bool childKnown = model.findEntity(relationship.childEntity) != nullptr;
        if (!parentKnown || !childKnown)
            continue;

        childrenOf[relationship.parentEntity].append(relationship.childEntity);
        indegree[relationship.childEntity] += 1;
    }

    // Deterministic queue: entities with no pending parents, in model order.
    QList<Id> ready;
    for (const Entity &entity : model.entities) {
        if (indegree.value(entity.id, 0) == 0)
            ready.append(entity.id);
    }

    while (!ready.isEmpty()) {
        const Id id = ready.takeFirst();
        order.append(id);
        for (const Id &child : childrenOf.value(id)) {
            const int remaining = indegree[child] - 1;
            indegree[child] = remaining;
            if (remaining == 0)
                ready.append(child);
        }
    }

    // Cycle members still have pending parents; append them in model order at the end.
    // Their FKs are ALTER-based, so this never produces a forward reference.
    QSet<Id> placed(order.cbegin(), order.cend());
    for (const Entity &entity : model.entities) {
        if (!placed.contains(entity.id))
            order.append(entity.id);
    }

    return order;
}

QString SqlGenerator::resolveType(const Entity &entity, const Column &column,
                                  const SqlDialect &dialect)
{
    // 1. explicit per-engine override
    const auto it = column.dialectOverrides.constFind(dialect.id());
    if (it != column.dialectOverrides.constEnd())
        return it.value();

    // 2. native hint when the target equals the hint's source engine
    if (!column.nativeType.isEmpty() && column.nativeType.engine == dialect.id())
        return column.nativeType.rawType;

    // 3. native enum type when the dialect supports it
    if (column.type.kind == CanonicalType::Kind::Enum) {
        const QString enumName = dialect.enumTypeName(entity, column);
        if (!enumName.isEmpty())
            return enumName;
    }

    // 4. canonical mapping
    return dialect.mapType(column.type).sqlType;
}

QList<TypeAdvisory> SqlGenerator::auditTypes(const Model &model, const SqlDialect &dialect)
{
    QList<TypeAdvisory> advisories;

    for (const Entity &entity : model.entities) {
        for (const Column &column : entity.columns) {
            // Accepted decisions are no longer advisories.
            if (column.dialectOverrides.contains(dialect.id()))
                continue;
            if (!column.nativeType.isEmpty() && column.nativeType.engine == dialect.id())
                continue;

            const TypeMapping mapping = dialect.mapType(column.type);
            if (!fidelityNeedsReview(mapping.fidelity))
                continue;

            // A dialect-native enum type is an exact representation (e.g. PostgreSQL
            // CREATE TYPE ... AS ENUM), so it never needs review.
            if (column.type.kind == CanonicalType::Kind::Enum
                && !dialect.enumTypeName(entity, column).isEmpty())
                continue;

            TypeAdvisory advisory;
            advisory.entityId = entity.id;
            advisory.columnId = column.id;
            advisory.entityName = entity.name;
            advisory.columnName = column.name;
            advisory.canonicalType = column.type.displayName();
            advisory.resolvedType = resolveType(entity, column, dialect);
            advisory.fidelity = mapping.fidelity;
            advisory.note = mapping.note;
            advisory.suggestedOverride = mapping.suggestedOverride;
            advisories.append(advisory);
        }
    }

    return advisories;
}

QList<QString> SqlGenerator::statements(const Model &model, const SqlDialect &dialect,
                                        const SqlGenOptions &options) const
{
    QList<QString> statements;
    const QList<Id> order = createOrder(model);

    // 1. drops: children first, so dropping never leaves an FK referencing a dropped table.
    if (options.emitDrop) {
        for (auto it = order.crbegin(); it != order.crend(); ++it) {
            const Entity *entity = model.findEntity(*it);
            if (entity)
                statements.append(dialect.dropTable(qualifiedName(*entity, dialect, options),
                                                    options.ifExists));
        }
    }

    // 2. pre-table statements (enum types, extensions...)
    statements += dialect.preTableStatements(model);

    // 3. create tables + comments
    for (const Id &id : order) {
        const Entity *entity = model.findEntity(id);
        if (!entity)
            continue;

        QStringList columnLines;
        QStringList quotedPkColumns;
        bool hasPrimaryKey = false;

        for (const Column &column : entity->columns) {
            const QString resolved = resolveType(*entity, column, dialect);
            columnLines.append(dialect.columnDefinition(column, resolved));
            if (column.primaryKey) {
                quotedPkColumns.append(dialect.quoteIdent(column.name));
                hasPrimaryKey = true;
            }
        }

        QString primaryKeyConstraint;
        if (hasPrimaryKey)
            primaryKeyConstraint = QStringLiteral("PRIMARY KEY (%1)")
                                       .arg(quotedPkColumns.join(QStringLiteral(", ")));

        statements.append(dialect.createTable(qualifiedName(*entity, dialect, options),
                                              columnLines, primaryKeyConstraint));

        if (options.includeComments) {
            if (!entity->comment.isEmpty()) {
                const QString statement = dialect.commentOn(
                    QStringLiteral("TABLE %1").arg(qualifiedName(*entity, dialect, options)),
                    entity->comment);
                if (!statement.isEmpty())
                    statements.append(statement);
            }
            for (const Column &column : entity->columns) {
                if (column.comment.isEmpty())
                    continue;
                const QString statement = dialect.commentOn(
                    QStringLiteral("COLUMN %1.%2")
                        .arg(qualifiedName(*entity, dialect, options),
                             dialect.quoteIdent(column.name)),
                    column.comment);
                if (!statement.isEmpty())
                    statements.append(statement);
            }
        }
    }

    // 4. foreign keys: always ALTER TABLE ADD CONSTRAINT, so cycle members are safe.
    for (const Relationship &relationship : model.relationships) {
        const Entity *child = model.findEntity(relationship.childEntity);
        const Entity *parent = model.findEntity(relationship.parentEntity);
        if (!child || !parent)
            continue;

        QStringList quotedChildColumns;
        for (const Id &columnId : relationship.childColumns) {
            const Column *column = child->findColumn(columnId);
            if (column)
                quotedChildColumns.append(dialect.quoteIdent(column->name));
        }
        QStringList quotedParentColumns;
        for (const Id &columnId : relationship.parentColumns) {
            const Column *column = parent->findColumn(columnId);
            if (column)
                quotedParentColumns.append(dialect.quoteIdent(column->name));
        }
        if (quotedChildColumns.isEmpty() || quotedParentColumns.isEmpty())
            continue;

        const QString constraintName = relationship.name.isEmpty()
                                           ? QStringLiteral("fk_%1_%2")
                                                 .arg(child->name, parent->name)
                                           : relationship.name;

        statements.append(dialect.addForeignKeyConstraint(
            qualifiedName(*child, dialect, options), constraintName, quotedChildColumns,
            qualifiedName(*parent, dialect, options), quotedParentColumns,
            relationship.onDelete, relationship.onUpdate));
    }

    // 5. indexes
    if (options.emitIndexes) {
        for (const Id &id : order) {
            const Entity *entity = model.findEntity(id);
            if (!entity)
                continue;
            for (const Index &index : entity->indexes) {
                QStringList quotedIndexColumns;
                for (const Id &columnId : index.columnIds) {
                    const Column *column = entity->findColumn(columnId);
                    if (column)
                        quotedIndexColumns.append(dialect.quoteIdent(column->name));
                }
                if (quotedIndexColumns.isEmpty())
                    continue;
                statements.append(dialect.createIndex(
                    qualifiedName(*entity, dialect, options),
                    indexNameFor(*entity, index, dialect), quotedIndexColumns, index.unique));
            }
        }
    }

    return statements;
}

QString SqlGenerator::generate(const Model &model, const SqlDialect &dialect,
                               const SqlGenOptions &options) const
{
    QStringList body;
    if (options.wrapInTransaction)
        body.append(QStringLiteral("BEGIN;"));
    body += statements(model, dialect, options);
    if (options.wrapInTransaction)
        body.append(QStringLiteral("COMMIT;"));

    const QString header = QStringLiteral(
                               "-- ------------------------------------------------------\n"
                               "-- beDBug forward engineering script\n"
                               "-- Target engine: %1\n"
                               "-- ------------------------------------------------------")
                               .arg(dialect.displayName());

    QString result = header + QStringLiteral("\n\n") + body.join(QStringLiteral("\n\n"));
    result += QLatin1Char('\n');
    return result;
}

} // namespace bb