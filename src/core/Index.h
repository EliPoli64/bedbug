#pragma once

#include "Id.h"

#include <QList>
#include <QString>

namespace bb {

struct Index
{
    Id id;
    QString name;
    QList<Id> columnIds;   // ordered
    bool unique = false;

    bool operator==(const Index &) const = default;
    bool operator!=(const Index &) const = default;
};

} // namespace bb