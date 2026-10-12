#pragma once

#include "Column.h"
#include "Id.h"
#include "Index.h"

#include <QList>
#include <QString>

namespace bb {

struct Entity
{
    Id id;
    QString name;      // table name
    QString schema;    // empty = engine default
    QString comment;
    QList<Column> columns;
    QList<Index> indexes;

    bool operator==(const Entity &) const = default;
    bool operator!=(const Entity &) const = default;

    const Column *findColumn(const Id &columnId) const;
    Column *findColumn(const Id &columnId);
};

} // namespace bb