#include "Entity.h"

namespace bb {

const Column *Entity::findColumn(const Id &columnId) const
{
    for (const Column &column : columns) {
        if (column.id == columnId)
            return &column;
    }
    return nullptr;
}

Column *Entity::findColumn(const Id &columnId)
{
    for (Column &column : columns) {
        if (column.id == columnId)
            return &column;
    }
    return nullptr;
}

} // namespace bb