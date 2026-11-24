#include "SqlServer.h"

namespace bb {

namespace {

// SQL Server's nvarchar/nchar maximum length is 4000 characters; anything longer must be
// declared as nvarchar(max) (PLAN.md §6).
constexpr int kMaxUnicodeLength = 4000;

// Referential actions SQL Server accepts. "RESTRICT" is standard SQL for "refuse the
// operation", which SQL Server spells NO ACTION.
QString referentialAction(const QString &action)
{
    if (action.compare(QStringLiteral("RESTRICT"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("NO ACTION");
    return action;
}

} // namespace

QString SqlServerDialect::id() const
{
    return QStringLiteral("sqlserver");
}

QString SqlServerDialect::displayName() const
{
    return QStringLiteral("SQL Server");
}

QString SqlServerDialect::quoteIdent(const QString &name) const
{
    QString escaped = name;
    escaped.replace(QLatin1Char(']'), QStringLiteral("]]"));
    return QStringLiteral("[%1]").arg(escaped);
}

QString SqlServerDialect::quoteLiteral(const QString &text) const
{
    // N'...' keeps the literal unicode; embedded quotes are doubled.
    QString escaped = text;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QStringLiteral("N'%1'").arg(escaped);
}

TypeMapping SqlServerDialect::mapType(const CanonicalType &type) const
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
        if (type.length <= 0)
            return {QStringLiteral("nvarchar(255)"), Fidelity::Narrowed,
                    QStringLiteral("no length specified"),
                    QStringLiteral("nvarchar(255)")};
        if (type.length > kMaxUnicodeLength)
            return {QStringLiteral("nvarchar(%1)").arg(kMaxUnicodeLength), Fidelity::Narrowed,
                    QStringLiteral("nvarchar is limited to 4000 characters"),
                    QStringLiteral("nvarchar(max)")};
        return {QStringLiteral("nvarchar(%1)").arg(type.length), Fidelity::Exact};
    }
    case K::Char: {
        if (type.length <= 0)
            return {QStringLiteral("nchar(1)"), Fidelity::Narrowed,
                    QStringLiteral("no length specified"), QStringLiteral("nchar(1)")};
        if (type.length > kMaxUnicodeLength)
            return {QStringLiteral("nchar(%1)").arg(kMaxUnicodeLength), Fidelity::Narrowed,
                    QStringLiteral("nchar is limited to 4000 characters"),
                    QStringLiteral("nvarchar(max)")};
        return {QStringLiteral("nchar(%1)").arg(type.length), Fidelity::Exact};
    }
    case K::Text:
        return {QStringLiteral("nvarchar(max)"), Fidelity::Widened};
    case K::Boolean:
        return {QStringLiteral("bit"), Fidelity::Exact};
    case K::Date:
        return {QStringLiteral("date"), Fidelity::Exact};
    case K::Time:
        return {QStringLiteral("time"), Fidelity::Exact};
    case K::Timestamp:
        return {QStringLiteral("datetime2"), Fidelity::Lossy,
                QStringLiteral("SQL Server datetime2 carries no time zone"),
                QStringLiteral("datetime2")};
    case K::Decimal: {
        if (type.precision > 0)
            return {QStringLiteral("decimal(%1,%2)").arg(type.precision).arg(type.scale),
                    Fidelity::Exact};
        return {QStringLiteral("decimal(18,2)"), Fidelity::Widened,
                QStringLiteral("no precision specified"),
                QStringLiteral("decimal(18,2)")};
    }
    case K::Float:
        return {QStringLiteral("real"), Fidelity::Exact};
    case K::Double:
        return {QStringLiteral("float"), Fidelity::Exact};
    case K::Blob:
        return {QStringLiteral("varbinary(max)"), Fidelity::Exact};
    case K::Json:
        return {QStringLiteral("nvarchar(max)"), Fidelity::Unsupported,
                QStringLiteral("SQL Server has no native JSON type"),
                QStringLiteral("nvarchar(max)")};
    case K::Uuid:
        return {QStringLiteral("uniqueidentifier"), Fidelity::Exact};
    case K::Enum: {
        // SQL Server has no enum type. A CHECK constraint would enforce the values; the
        // generated column simply keeps them as text until the user decides otherwise.
        if (type.enumValues.isEmpty())
            return {QStringLiteral("nvarchar(255)"), Fidelity::Lossy,
                    QStringLiteral("enum without values drawn as text"),
                    QStringLiteral("nvarchar(255)")};
        return {QStringLiteral("nvarchar(255)"), Fidelity::Lossy,
                QStringLiteral("SQL Server has no enum type; values are not enforced"),
                QStringLiteral("nvarchar(255)")};
    }
    }
    return {QStringLiteral("nvarchar(max)"), Fidelity::Unsupported,
            QStringLiteral("unmapped canonical type")};
}

QStringList SqlServerDialect::preTableStatements(const Model &model) const
{
    Q_UNUSED(model);
    return {};
}

QString SqlServerDialect::columnDefinition(const Column &column,
                                           const QString &resolvedType) const
{
    QStringList parts;
    parts.append(quoteIdent(column.name));
    parts.append(resolvedType);
    if (!column.nullable)
        parts.append(QStringLiteral("NOT NULL"));
    if (column.autoIncrement)
        parts.append(QStringLiteral("IDENTITY(1,1)"));
    if (column.unique)
        parts.append(QStringLiteral("UNIQUE"));
    if (!column.defaultValue.isEmpty())
        parts.append(QStringLiteral("DEFAULT ") + column.defaultValue);
    return parts.join(QLatin1Char(' '));
}

QString SqlServerDialect::createTable(const QString &qualifiedName,
                                      const QStringList &columnLines,
                                      const QString &primaryKeyConstraint) const
{
    QStringList lines = columnLines;
    if (!primaryKeyConstraint.isEmpty())
        lines.append(primaryKeyConstraint);

    return QStringLiteral("CREATE TABLE %1 (\n  %2\n);")
        .arg(qualifiedName, lines.join(QStringLiteral(",\n  ")));
}

QString SqlServerDialect::addForeignKeyConstraint(const QString &qualifiedChildTable,
                                                  const QString &constraintName,
                                                  const QStringList &quotedChildColumns,
                                                  const QString &qualifiedParentTable,
                                                  const QStringList &quotedParentColumns,
                                                  const QString &onDelete,
                                                  const QString &onUpdate) const
{
    QString action;
    if (!onDelete.isEmpty())
        action += QStringLiteral(" ON DELETE ") + referentialAction(onDelete);
    if (!onUpdate.isEmpty())
        action += QStringLiteral(" ON UPDATE ") + referentialAction(onUpdate);

    return QStringLiteral("ALTER TABLE %1\n  ADD CONSTRAINT %2 FOREIGN KEY (%3)\n"
                          "  REFERENCES %4 (%5)%6;")
        .arg(qualifiedChildTable, quoteIdent(constraintName),
             quotedChildColumns.join(QStringLiteral(", ")), qualifiedParentTable,
             quotedParentColumns.join(QStringLiteral(", ")), action);
}

QString SqlServerDialect::createIndex(const QString &qualifiedTableName,
                                      const QString &indexName,
                                      const QStringList &quotedColumns,
                                      bool unique) const
{
    return QStringLiteral("CREATE %1INDEX %2 ON %3 (%4);")
        .arg(unique ? QStringLiteral("UNIQUE ") : QString(), quoteIdent(indexName),
             qualifiedTableName, quotedColumns.join(QStringLiteral(", ")));
}

QString SqlServerDialect::dropTable(const QString &qualifiedName, bool ifExists) const
{
    return QStringLiteral("DROP TABLE %1%2;")
        .arg(ifExists ? QStringLiteral("IF EXISTS ") : QString(), qualifiedName);
}

QString SqlServerDialect::commentOn(const QString &object, const QString &comment) const
{
    Q_UNUSED(object);
    Q_UNUSED(comment);
    // SQL Server has no COMMENT ON; comments would need sp_addextendedproperty calls,
    // which are deliberately out of scope for the generated DDL.
    return {};
}

} // namespace bb
