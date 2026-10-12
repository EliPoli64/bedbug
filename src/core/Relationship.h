#pragma once

#include "Id.h"

#include <QList>
#include <QString>

namespace bb {

// Describes the shape a relationship takes at creation time (PLAN.md §8).
enum class RelationshipKind
{
    OneToOne,   // FK + unique child column(s); drawn with a "one" mark at both ends
    OneToMany,  // plain FK (crow's foot at the child end)
    ManyToMany, // materializes as a junction table with two FKs
};

// A relationship is exactly one foreign key (PLAN.md §4).
struct Relationship
{
    Id id;
    QString name;
    Id childEntity;            // table owning the foreign key
    QList<Id> childColumns;    // columns forming the FK (ordered)
    Id parentEntity;           // referenced table
    QList<Id> parentColumns;   // referenced columns (ordered)
    bool identifying = false;  // weak entity relationship; drawn solid
    QString onDelete;          // "CASCADE" | "RESTRICT" | "SET NULL" | "NO ACTION" | ""
    QString onUpdate;

    bool operator==(const Relationship &) const = default;
    bool operator!=(const Relationship &) const = default;
};

} // namespace bb