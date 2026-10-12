#pragma once

#include "Entity.h"
#include "Id.h"
#include "Relationship.h"

#include <QList>
#include <QPointF>
#include <QString>

namespace bb {

struct DiagramNode
{
    Id entity;
    QPointF pos;   // scene coordinates in this diagram

    bool operator==(const DiagramNode &) const = default;
    bool operator!=(const DiagramNode &) const = default;
};

struct Diagram
{
    Id id;
    QString name;
    QList<DiagramNode> nodes;
    double zoom = 1.0;

    bool operator==(const Diagram &) const = default;
    bool operator!=(const Diagram &) const = default;
};

struct Model
{
    Model();

    QString name;              // document title
    QString formatVersion;     // format of this document
    QString preferredDialect;  // convenience default (PLAN.md §7.1); never gates generation
    QList<Entity> entities;
    QList<Relationship> relationships;
    QList<Diagram> diagrams;

    bool operator==(const Model &) const = default;
    bool operator!=(const Model &) const = default;

    Entity *findEntity(const Id &entityId);
    const Entity *findEntity(const Id &entityId) const;

    // Relationships where the given entity is either the child or the parent.
    QList<Relationship> relationshipsFor(const Id &entityId) const;

    static QString currentFormatVersion();
};

} // namespace bb