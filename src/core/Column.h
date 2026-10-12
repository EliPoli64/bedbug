#pragma once

#include "CanonicalType.h"
#include "Id.h"

#include <QHash>
#include <QString>

namespace bb {

// The original native type of a column plus the engine it was authored in.
// Used for exact round-trips when exporting to the same engine (PLAN.md §7.6).
struct DialectHint
{
    QString rawType;   // e.g. "jsonb"
    QString engine;    // dialect id, e.g. "postgres"

    bool operator==(const DialectHint &) const = default;
    bool operator!=(const DialectHint &) const = default;

    bool isEmpty() const { return rawType.isEmpty() && engine.isEmpty(); }
};

struct Column
{
    Id id;
    QString name;
    CanonicalType type;
    bool nullable = true;
    bool primaryKey = false;
    bool unique = false;
    bool autoIncrement = false;
    QString defaultValue;   // raw SQL expression, e.g. "CURRENT_TIMESTAMP"
    QString comment;

    // dialect id -> raw type string, e.g. { "sqlserver": "nvarchar(4000)" }
    QHash<QString, QString> dialectOverrides;
    DialectHint nativeType;

    bool operator==(const Column &) const = default;
    bool operator!=(const Column &) const = default;
};

} // namespace bb