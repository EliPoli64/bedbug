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

    // --- File lifecycle -------------------------------------------------------

    // Loads a document from disk, adopting it as the current file. Returns false with
    // a populated errorMessage on failure.
    bool loadFromFile(const QString &path, QString *errorMessage = nullptr);

    // Writes the document and adopts it as the current file (clears the dirty flag).
    bool saveToFile(const QString &path, QString *errorMessage = nullptr);

    // Session autosave: reads/writes the current model without changing the document
    // file path or dirty flag (used for restore-on-startup).
    bool loadSession(const QString &path, QString *errorMessage = nullptr);
    bool saveSession(const QString &path, QString *errorMessage = nullptr) const;

    // Resets to a fresh document with one empty diagram.
    void newDocument();

    // --- Working-space ("sheet") grid ---------------------------------------

    // Adds one A4 sheet to the active diagram in the given direction and emits
    // modelChanged().
    void addSheetLeft();
    void addSheetRight();
    void addSheetUp();
    void addSheetDown();

    // Removes the last sheet on the given side, but only when more than one sheet
    // remains in that direction and the sheet contains no entity nodes.
    void removeSheetLeft();
    void removeSheetRight();
    void removeSheetUp();
    void removeSheetDown();

    // Marks the document dirty without rebuilding (used by canvas auto-grow).
    void markDirty();

    bool isModified() const { return m_modified; }
    bool hasFile() const { return !m_filePath.isEmpty(); }
    QString filePath() const { return m_filePath; }

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

    // Resolves the columns for a 1:1 / 1:N relationship and creates it:
    //  - parent column: the explicit column, else the parent's primary key (or first).
    //  - child column: the explicit column, else a column named "{parent}_id" — reused if
    //    present, otherwise created with the parent column's type.
    // Returns nullptr on failure (the request is dropped) and emits modelChanged() once.
    Relationship *addRelationshipPick(RelationshipKind kind, const Id &childEntityId,
                                      const Id &childColumnId, const Id &parentEntityId,
                                      const Id &parentColumnId, const QString &name = {});

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
    void markModifiedAndNotify();

    Id m_activeDiagramId;
    Model m_model;
    QString m_filePath;
    bool m_modified = false;
};

} // namespace bb