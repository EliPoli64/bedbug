#include "Postgres.h"

#include <QSet>

namespace bb {

namespace {

QString lowerSanitized(QString text)
{
    text = text.toLower();
    for (QChar &c : text) {
        if (!(c.isLower() && c.isLetter()) && !(c >= QLatin1Char('0') && c <= QLatin1Char('9'))
            && c != QLatin1Char('_'))
            c = QLatin1Char('_');
    }
    return text;
}

} // namespace

QString PostgresDialect::id() const
{
    return QStringLiteral("postgres");
}

QString PostgresDialect::displayName() const
{
    return QStringLiteral("PostgreSQL");
}

QString PostgresDialect::quoteIdent(const QString &name) const
{
    QString escaped = name;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

QString PostgresDialect::quoteLiteral(const QString &text) const
{
    QString escaped = text;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QStringLiteral("'%1'").arg(escaped);
}

TypeMapping PostgresDialect::mapType(const CanonicalType &type) const
{
    using K = CanonicalType::Kind;

    switch (type.kind) {
    case K::Int:
        return {QStringLiteral("integer"), Fidelity::Exact};
    case K::BigInt:
        return {QStringLiteral("bigint"), Fidelity::Exact};
    case K::SmallInt:
        return {QStringLiteral("smallint"), Fidelity::Exact};
    case K::Varchar: {
        if (type.length > 0)
            return {QStringLiteral("varchar(%1)").arg(type.length), Fidelity::Exact};
        return {QStringLiteral("varchar"), Fidelity::Narrowed,
                QStringLiteral("no length specified"), QStringLiteral("varchar(255)")};
    }
    case K::Char: {
        if (type.length > 0)
            return {QStringLiteral("char(%1)").arg(type.length), Fidelity::Exact};
        return {QStringLiteral("char"), Fidelity::Narrowed, QStringLiteral("no length specified"),
                QStringLiteral("char(1)")};
    }
    case K::Text:
        return {QStringLiteral("text"), Fidelity::Exact};
    case K::Boolean:
        return {QStringLiteral("boolean"), Fidelity::Exact};
    case K::Date:
        return {QStringLiteral("date"), Fidelity::Exact};
    case K::Time:
        return {QStringLiteral("time"), Fidelity::Exact};
    case K::Timestamp:
        return {QStringLiteral("timestamptz"), Fidelity::Exact};
    case K::Decimal: {
        if (type.precision > 0)
            return {QStringLiteral("numeric(%1,%2)").arg(type.precision).arg(type.scale),
                    Fidelity::Exact};
        return {QStringLiteral("numeric"), Fidelity::Widened,
                QStringLiteral("no precision specified"), QStringLiteral("numeric(18,2)")};
    }
    case K::Float:
        return {QStringLiteral("real"), Fidelity::Exact};
    case K::Double:
        return {QStringLiteral("double precision"), Fidelity::Exact};
    case K::Blob:
        return {QStringLiteral("bytea"), Fidelity::Exact};
    case K::Json:
        return {QStringLiteral("jsonb"), Fidelity::Exact};
    case K::Uuid:
        return {QStringLiteral("uuid"), Fidelity::Exact};
    case K::Enum:
        // Handled via enumTypeName() + preTableStatements(); reaching here means the
        // generator fell back because no native enum type name could be derived.
        return {QStringLiteral("text"), Fidelity::Lossy,
                QStringLiteral("enum drawn as text without a native type"),
                QStringLiteral("text")};
    }
    return {QStringLiteral("text"), Fidelity::Unsupported,
            QStringLiteral("unmapped canonical type")};
}

QString PostgresDialect::enumTypeName(const Entity &entity, const Column &column) const
{
    if (column.type.kind != CanonicalType::Kind::Enum)
        return {};
    return lowerSanitized(entity.name) + QLatin1Char('_') + lowerSanitized(column.name)
           + QStringLiteral("_enum");
}

QStringList PostgresDialect::preTableStatements(const Model &model) const
{
    QList<QString> statements;
    QSet<QString> emitted;

    for (const Entity &entity : model.entities) {
        for (const Column &column : entity.columns) {
            if (column.type.kind != CanonicalType::Kind::Enum)
                continue;

            const QString typeName = enumTypeName(entity, column);
            if (typeName.isEmpty() || emitted.contains(typeName))
                continue;
            emitted.insert(typeName);

            QStringList values;
            for (const QString &value : column.type.enumValues)
                values.append(quoteLiteral(value));

            statements.append(QStringLiteral("CREATE TYPE %1 AS ENUM (%2);")
                                  .arg(quoteIdent(typeName), values.join(QStringLiteral(", "))));
        }
    }

    return statements;
}

QString PostgresDialect::columnDefinition(const Column &column,
                                          const QString &resolvedType) const
{
    QStringList parts;
    parts.append(quoteIdent(column.name));
    parts.append(resolvedType);
    if (!column.nullable)
        parts.append(QStringLiteral("NOT NULL"));
    if (column.unique)
        parts.append(QStringLiteral("UNIQUE"));
    if (column.autoIncrement)
        parts.append(QStringLiteral("GENERATED BY DEFAULT AS IDENTITY"));
    if (!column.defaultValue.isEmpty())
        parts.append(QStringLiteral("DEFAULT ") + column.defaultValue);
    return parts.join(QLatin1Char(' '));
}

QString PostgresDialect::createTable(const QString &qualifiedName,
                                     const QStringList &columnLines,
                                     const QString &primaryKeyConstraint) const
{
    QStringList lines = columnLines;
    if (!primaryKeyConstraint.isEmpty())
        lines.append(primaryKeyConstraint);

    return QStringLiteral("CREATE TABLE %1 (\n  %2\n);")
        .arg(qualifiedName, lines.join(QStringLiteral(",\n  ")));
}

QString PostgresDialect::addForeignKeyConstraint(const QString &qualifiedChildTable,
                                                 const QString &constraintName,
                                                 const QStringList &quotedChildColumns,
                                                 const QString &qualifiedParentTable,
                                                 const QStringList &quotedParentColumns,
                                                 const QString &onDelete,
                                                 const QString &onUpdate) const
{
    QString action;
    if (!onDelete.isEmpty())
        action += QStringLiteral(" ON DELETE ") + onDelete;
    if (!onUpdate.isEmpty())
        action += QStringLiteral(" ON UPDATE ") + onUpdate;

    return QStringLiteral("ALTER TABLE %1\n  ADD CONSTRAINT %2 FOREIGN KEY (%3)\n  REFERENCES %4 (%5)%6;")
        .arg(qualifiedChildTable, quoteIdent(constraintName),
             quotedChildColumns.join(QStringLiteral(", ")), qualifiedParentTable,
             quotedParentColumns.join(QStringLiteral(", ")), action);
}

QString PostgresDialect::createIndex(const QString &qualifiedTableName,
                                     const QString &indexName,
                                     const QStringList &quotedColumns,
                                     bool unique) const
{
    return QStringLiteral("CREATE %1INDEX %2 ON %3 (%4);")
        .arg(unique ? QStringLiteral("UNIQUE ") : QString(), quoteIdent(indexName),
             qualifiedTableName, quotedColumns.join(QStringLiteral(", ")));
}

QString PostgresDialect::dropTable(const QString &qualifiedName, bool ifExists) const
{
    return QStringLiteral("DROP TABLE %1%2;")
        .arg(ifExists ? QStringLiteral("IF EXISTS ") : QString(), qualifiedName);
}

QString PostgresDialect::commentOn(const QString &object, const QString &comment) const
{
    return QStringLiteral("COMMENT ON %1 IS %2;").arg(object, quoteLiteral(comment));
}

} // namespace bb