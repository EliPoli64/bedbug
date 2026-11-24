#pragma once

#include "Model.h"

#include <QJsonObject>
#include <QObject>
#include <QUndoStack>

#include <optional>

namespace bb {

class ModelSnapshotCommand;
class MoveEntityNodeCommand;

// One accepted per-engine type decision (PLAN.md §7.5). Applied as a single undo step.
struct ColumnTypeOverride
{
    Id entityId;
    Id columnId;
    QString rawType;
};

// Owns the in-memory document (Model + active diagram) and coordinates mutations.
// Every mutation is applied through a QUndoStack command, so nothing bypasses undo.
// The modified flag is derived from the stack's clean state plus any out-of-band change
// (canvas auto-grow).
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

    // Undo/redo history. The shell creates its Undo/Redo actions from this stack.
    QUndoStack *undoStack() { return &m_undoStack; }
    const QUndoStack *undoStack() const { return &m_undoStack; }

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

    // Sets the active diagram's page grid to the given size (origin reset to 0,0)
    // and emits modelChanged().
    void setCanvasSize(int pagesX, int pagesY);

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

    // Creates an entity plus a diagram node, positioned at `position` when given and in
    // a cascading grid otherwise. Emits modelChanged().
    Entity *addEntity(const QString &name, std::optional<QPointF> position = std::nullopt);

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

    // Writes `dialectOverrides[dialectId] = rawType` for every entry as one undoable
    // command ("Resolve types"). Entries that are already decided are skipped. Returns
    // the number of columns changed; 0 leaves the document (and undo stack) untouched.
    int applyTypeOverrides(const QString &dialectId,
                           const QList<ColumnTypeOverride> &overrides);

    // Writes a moved table's node position back to the active diagram. Consecutive moves
    // of the same table merge into a single undo step.
    void moveEntityNode(const Id &entityId, const QPointF &position);

signals:
    void modelChanged();
    void modifiedChanged(bool modified);

private:
    // Undo-command hooks: restore a whole document snapshot, or one node position.
    void applySnapshot(const QJsonObject &snapshot);
    void applyNodePosition(const Id &diagramId, const Id &entityId, const QPointF &position);

    void commit(const QJsonObject &before, const QString &text);
    void refreshModified();
    void resetHistory();

    Id m_activeDiagramId;
    Model m_model;
    QString m_filePath;
    QUndoStack m_undoStack;
    bool m_modified = false;
    bool m_externalDirty = false;

    friend class ModelSnapshotCommand;
    friend class MoveEntityNodeCommand;
};

} // namespace bb
