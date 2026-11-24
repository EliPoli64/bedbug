#include "MySql.h"

namespace bb {

QString MySqlDialect::id() const
{
    return QStringLiteral("mysql");
}

QString MySqlDialect::displayName() const
{
    return QStringLiteral("MySQL");
}

QString MySqlDialect::quoteIdent(const QString &name) const
{
    QString escaped = name;
    escaped.replace(QLatin1Char('`'), QStringLiteral("``"));
    return QStringLiteral("`%1`").arg(escaped);
}

QString MySqlDialect::quoteLiteral(const QString &text) const
{
    // Escape backslashes first: MySQL treats them as escape characters by default.
    QString escaped = text;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QStringLiteral("'%1'").arg(escaped);
}

TypeMapping MySqlDialect::mapType(const CanonicalType &type) const
{
    using K = CanonicalType::Kind;

    switch (type.kind) {
    case K::Int:
        return {QStringLiteral("int"), Fidelity::Exact};
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
        return {QStringLiteral("text"), Fidelity::Narrowed,
                QStringLiteral("MySQL TEXT is limited to 64 KB"),
                QStringLiteral("longtext")};
    case K::Boolean:
        return {QStringLiteral("tinyint(1)"), Fidelity::Exact,
                QStringLiteral("MySQL has no boolean type; tinyint(1) is the convention")};
    case K::Date:
        return {QStringLiteral("date"), Fidelity::Exact};
    case K::Time:
        return {QStringLiteral("time"), Fidelity::Exact};
    case K::Timestamp:
        return {QStringLiteral("timestamp"), Fidelity::Exact};
    case K::Decimal: {
        if (type.precision > 0)
            return {QStringLiteral("decimal(%1,%2)").arg(type.precision).arg(type.scale),
                    Fidelity::Exact};
        return {QStringLiteral("decimal"), Fidelity::Widened,
                QStringLiteral("no precision specified"), QStringLiteral("decimal(18,2)")};
    }
    case K::Float:
        return {QStringLiteral("float"), Fidelity::Exact};
    case K::Double:
        return {QStringLiteral("double"), Fidelity::Exact};
    case K::Blob:
        return {QStringLiteral("blob"), Fidelity::Narrowed,
                QStringLiteral("MySQL BLOB is limited to 64 KB"),
                QStringLiteral("longblob")};
    case K::Json:
        return {QStringLiteral("json"), Fidelity::Exact};
    case K::Uuid:
        return {QStringLiteral("char(36)"), Fidelity::Lossy,
                QStringLiteral("MySQL has no native UUID type"),
                QStringLiteral("char(36)")};
    case K::Enum: {
        if (type.enumValues.isEmpty())
            return {QStringLiteral("text"), Fidelity::Lossy,
                    QStringLiteral("enum without values drawn as text"),
                    QStringLiteral("text")};

        QStringList values;
        values.reserve(type.enumValues.size());
        for (const QString &value : type.enumValues)
            values.append(quoteLiteral(value));
        return {QStringLiteral("enum(%1)").arg(values.join(QStringLiteral(", "))),
                Fidelity::Exact};
    }
    }
    return {QStringLiteral("text"), Fidelity::Unsupported,
            QStringLiteral("unmapped canonical type")};
}

QStringList MySqlDialect::preTableStatements(const Model &model) const
{
    Q_UNUSED(model);
    return {};
}

QString MySqlDialect::columnDefinition(const Column &column,
                                       const QString &resolvedType) const
{
    QStringList parts;
    parts.append(quoteIdent(column.name));
    parts.append(resolvedType);
    if (!column.nullable)
        parts.append(QStringLiteral("NOT NULL"));
    if (column.autoIncrement)
        parts.append(QStringLiteral("AUTO_INCREMENT"));
    if (column.unique)
        parts.append(QStringLiteral("UNIQUE"));
    if (!column.defaultValue.isEmpty())
        parts.append(QStringLiteral("DEFAULT ") + column.defaultValue);
    if (!column.comment.isEmpty())
        parts.append(QStringLiteral("COMMENT ") + quoteLiteral(column.comment));
    return parts.join(QLatin1Char(' '));
}

QString MySqlDialect::createTable(const QString &qualifiedName,
                                  const QStringList &columnLines,
                                  const QString &primaryKeyConstraint) const
{
    QStringList lines = columnLines;
    if (!primaryKeyConstraint.isEmpty())
        lines.append(primaryKeyConstraint);

    return QStringLiteral("CREATE TABLE %1 (\n  %2\n);")
        .arg(qualifiedName, lines.join(QStringLiteral(",\n  ")));
}

QString MySqlDialect::addForeignKeyConstraint(const QString &qualifiedChildTable,
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

QString MySqlDialect::createIndex(const QString &qualifiedTableName,
                                  const QString &indexName,
                                  const QStringList &quotedColumns,
                                  bool unique) const
{
    return QStringLiteral("CREATE %1INDEX %2 ON %3 (%4);")
        .arg(unique ? QStringLiteral("UNIQUE ") : QString(), quoteIdent(indexName),
             qualifiedTableName, quotedColumns.join(QStringLiteral(", ")));
}

QString MySqlDialect::dropTable(const QString &qualifiedName, bool ifExists) const
{
    return QStringLiteral("DROP TABLE %1%2;")
        .arg(ifExists ? QStringLiteral("IF EXISTS ") : QString(), qualifiedName);
}

QString MySqlDialect::commentOn(const QString &object, const QString &comment) const
{
    Q_UNUSED(object);
    Q_UNUSED(comment);
    // MySQL has no COMMENT ON; column comments are emitted inline by columnDefinition().
    return {};
}

} // namespace bb
