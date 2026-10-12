#pragma once

#include "Model.h"

#include <QObject>

namespace bb {

// Owns the in-memory document (Model + active diagram) and coordinates mutations.
// For M3 there is no undo stack yet (M5) and no file persistence yet (M6); both plug in
// here.
class DocumentController : public QObject
{
    Q_OBJECT

public:
    explicit DocumentController(QObject *parent = nullptr);

    Model *model() { return &m_model; }
    const Model *model() const { return &m_model; }

    Diagram *activeDiagram();
    const Diagram *activeDiagram() const;
    Id activeDiagramId() const { return m_activeDiagramId; }

    // Creates an entity plus a diagram node, positioned in a cascading grid, and emits
    // modelChanged().
    Entity *addEntity(const QString &name);

    // Creates a foreign-key relationship between columns of two distinct entities and
    // emits modelChanged(). Returns nullptr when either entity or any column is unknown,
    // when both sides live on the same entity, or when the kind is ManyToMany.
    Relationship *addRelationship(RelationshipKind kind, const Id &childEntityId,
                                  const QList<Id> &childColumnIds, const Id &parentEntityId,
                                  const QList<Id> &parentColumnIds, const QString &name = {});

    // Convenience for OneToMany relationships.
    Relationship *addRelationship(const Id &childEntityId, const QList<Id> &childColumnIds,
                                  const Id &parentEntityId, const QList<Id> &parentColumnIds,
                                  const QString &name = {});

    // Builds a junction table ("left_right") with a composite PK of two FKs to both
    // tables, places it between them on the diagram, and emits modelChanged(). Returns
    // nullptr when either table is missing or has no referenceable columns.
    Entity *createManyToMany(const Id &leftEntityId, const Id &rightEntityId);

    // Removes an entity, all its diagram nodes, and every relationship touching it.
    bool removeEntity(const Id &entityId);

    // Removes a column, any relationship referencing it, and any index referencing it.
    bool removeColumn(const Id &entityId, const Id &columnId);

    // Renames an entity (empty names are ignored).
    bool renameEntity(const Id &entityId, const QString &name);

    // Appends a fully specified column to an entity and emits modelChanged().
    Column *addColumn(const Id &entityId, const QString &name, const CanonicalType &type,
                      bool nullable, bool primaryKey, bool unique, bool autoIncrement,
                      const QString &defaultValue = {}, const QString &comment = {});

    // Replaces an existing column's definition (its id is preserved) and emits
    // modelChanged(). Returns false when the entity or column is unknown.
    bool updateColumn(const Id &entityId, const Column &column);

signals:
    void modelChanged();

private:
    Id m_activeDiagramId;
    Model m_model;
};

} // namespace bb