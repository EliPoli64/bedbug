#pragma once

#include "SqlDialect.h"

#include <QList>
#include <QString>

namespace bb {

// One column whose canonical type the target engine cannot represent exactly. These drive
// the fidelity chip and the "Resolve all" action (PLAN.md §7.3, §7.8).
struct TypeAdvisory
{
    Id entityId;
    Id columnId;
    QString entityName;
    QString columnName;
    QString canonicalType;    // human-readable, e.g. "json"
    QString resolvedType;     // what generation currently emits
    Fidelity fidelity = Fidelity::Exact;
    QString note;             // why it matters
    QString suggestedOverride; // alternative the dialect recommends, when any
};

// Turns a Model into a dialect-specific DDL script (PLAN.md §6).
class SqlGenerator
{
public:
    // Full script (header + statements, joined) for writing to a file.
    QString generate(const Model &model,
                     const SqlDialect &dialect,
                     const SqlGenOptions &options = {}) const;

    // The individual, fully-formed statements (without header/transaction wrapper).
    QList<QString> statements(const Model &model,
                              const SqlDialect &dialect,
                              const SqlGenOptions &options = {}) const;

    // Topological order of entities so that parents precede children. Entities in FK
    // cycles are appended at the end; all FKs are emitted as ALTER statements, so the
    // order never causes forward references.
    static QList<Id> createOrder(const Model &model);

    // PLAN.md §7.5 resolution order: per-engine override > native hint for the same
    // engine > canonical mapping (falling back to mapType when the dialect has no
    // native enum type or nothing else applies).
    static QString resolveType(const Entity &entity,
                               const Column &column,
                               const SqlDialect &dialect);

    // Every column whose mapping needs review for this engine. Columns with an explicit
    // override or a matching native hint are considered decided and are not reported.
    // Widened mappings are lossless and stay silent.
    static QList<TypeAdvisory> auditTypes(const Model &model, const SqlDialect &dialect);
};

} // namespace bb