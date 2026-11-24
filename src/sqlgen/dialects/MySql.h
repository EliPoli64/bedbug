#pragma once

#include "SqlDialect.h"

namespace bb {

class MySqlDialect final : public SqlDialect
{
public:
    QString id() const override;
    QString displayName() const override;
    QString quoteIdent(const QString &name) const override;
    QString quoteLiteral(const QString &text) const override;
    TypeMapping mapType(const CanonicalType &type) const override;
    QStringList preTableStatements(const Model &model) const override;
    QString columnDefinition(const Column &column, const QString &resolvedType) const override;
    QString createTable(const QString &qualifiedName,
                        const QStringList &columnLines,
                        const QString &primaryKeyConstraint) const override;
    QString addForeignKeyConstraint(const QString &qualifiedChildTable,
                                    const QString &constraintName,
                                    const QStringList &quotedChildColumns,
                                    const QString &qualifiedParentTable,
                                    const QStringList &quotedParentColumns,
                                    const QString &onDelete,
                                    const QString &onUpdate) const override;
    QString createIndex(const QString &qualifiedTableName,
                        const QString &indexName,
                        const QStringList &quotedColumns,
                        bool unique) const override;
    QString dropTable(const QString &qualifiedName, bool ifExists) const override;
    QString commentOn(const QString &object, const QString &comment) const override;
};

} // namespace bb
