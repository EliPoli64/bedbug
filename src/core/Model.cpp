#include "Model.h"

namespace bb {

Model::Model()
{
    formatVersion = currentFormatVersion();
}

Entity *Model::findEntity(const Id &entityId)
{
    for (Entity &entity : entities) {
        if (entity.id == entityId)
            return &entity;
    }
    return nullptr;
}

const Entity *Model::findEntity(const Id &entityId) const
{
    for (const Entity &entity : entities) {
        if (entity.id == entityId)
            return &entity;
    }
    return nullptr;
}

QList<Relationship> Model::relationshipsFor(const Id &entityId) const
{
    QList<Relationship> result;
    for (const Relationship &relationship : relationships) {
        if (relationship.childEntity == entityId || relationship.parentEntity == entityId)
            result.append(relationship);
    }
    return result;
}

QString Model::currentFormatVersion()
{
    return QStringLiteral("1");
}

} // namespace bb