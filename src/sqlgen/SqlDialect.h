#pragma once

#include "CanonicalType.h"
#include "Column.h"
#include "Entity.h"
#include "Model.h"

#include <QString>
#include <QStringList>

namespace bb {

// How faithful a canonical -> dialect type mapping is (PLAN.md §7.3).
enum class Fidelity
{
    Exact,       // 1:1, no semantic change
    Widened,     // lossless superset (e.g. Text -> nvarchar(max))
    Narrowed,    // parameter clamped to fit the target
    Lossy,       // semantics change
    Unsupported, // no native type; needs a decision
};

struct TypeMapping
{
    QString sqlType;
    Fidelity fidelity = Fidelity::Exact;
    QString note;               // human-readable explanation
    QString suggestedOverride;  // e.g. "nvarchar(max)"; empty when none applies
};

// Options that shape generated scripts (PLAN.md §6).
struct SqlGenOptions
{
    bool emitDrop = false;          // emit DROP TABLE statements first
    bool ifExists = false;          // ... IF EXISTS
    bool wrapInTransaction = false; // BEGIN; ... COMMIT;
    bool emitIndexes = true;        // emit CREATE [UNIQUE] INDEX statements
    bool qualifySchema = true;      // prefix tables with their schema when set
    bool includeComments = true;    // emit COMMENT ON statements
};

// Abstraction over one target SQL engine. Each dialect formats *its own* syntax;
// type resolution, ordering, and name quoting policy live in SqlGenerator.
class SqlDialect
{
public:
    virtual ~SqlDialect() = default;

    virtual QString id() const = 0;          // "postgres"
    virtual QString displayName() const = 0; // "PostgreSQL"

    // Quote one identifier for this dialect ("name" for PostgreSQL).
    virtual QString quoteIdent(const QString &name) const = 0;

    // Quote a string literal for this dialect ('value' with '' escaping).
    virtual QString quoteLiteral(const QString &text) const = 0;

    // Canonical -> concrete SQL type, including fidelity.
    virtual TypeMapping mapType(const CanonicalType &type) const = 0;

    // Type name for an enum column that needs a native enum type, or empty when the
    // dialect has no native enum (generator then falls back to mapType).
    virtual QString enumTypeName(const Entity &entity, const Column &column) const
    {
        Q_UNUSED(entity);
        Q_UNUSED(column);
        return {};
    }

    // Statements emitted before any CREATE TABLE (e.g. CREATE TYPE ... AS ENUM).
    virtual QStringList preTableStatements(const Model &model) const = 0;

    // One column definition line. `resolvedType` is already mapped by the generator.
    virtual QString columnDefinition(const Column &column,
                                     const QString &resolvedType) const = 0;

    virtual QString createTable(const QString &qualifiedName,
                                const QStringList &columnLines,
                                const QString &primaryKeyConstraint) const = 0;

    virtual QString addForeignKeyConstraint(const QString &qualifiedChildTable,
                                            const QString &constraintName,
                                            const QStringList &quotedChildColumns,
                                            const QString &qualifiedParentTable,
                                            const QStringList &quotedParentColumns,
                                            const QString &onDelete,
                                            const QString &onUpdate) const = 0;

    virtual QString createIndex(const QString &qualifiedTableName,
                                const QString &indexName,
                                const QStringList &quotedColumns,
                                bool unique) const = 0;

    virtual QString dropTable(const QString &qualifiedName, bool ifExists) const = 0;

    // COMMENT ON statement. `object` is e.g. "TABLE \"public\".\"customer\"". Only called
    // when the model carries relevant comments.
    virtual QString commentOn(const QString &object, const QString &comment) const = 0;
};

} // namespace bb