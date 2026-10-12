#pragma once

#include "Id.h"

#include <QList>
#include <QString>

namespace bb {

// A relationship is exactly one foreign key (PLAN.md §4).
struct Relationship
{
    Id id;
    QString name;
    Id childEntity;            // table owning the foreign key
    QList<Id> childColumns;    // columns forming the FK (ordered)
    Id parentEntity;           // referenced table
    QList<Id> parentColumns;   // referenced columns (ordered)
    bool identifying = false;  // weak entity relationship
    QString onDelete;          // "CASCADE" | "RESTRICT" | "SET NULL" | "NO ACTION" | ""
    QString onUpdate;

    bool operator==(const Relationship &) const = default;
    bool operator!=(const Relationship &) const = default;
};

} // namespace bb