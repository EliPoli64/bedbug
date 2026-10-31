#pragma once

#include <QJsonObject>
#include <QUndoCommand>

namespace bb {

class DocumentController;

// Snapshot-based undo command. DocumentController applies each mutation eagerly (so
// callers can keep using the returned pointers) and records the before/after model as
// serialized JSON here. JSON is used instead of copying Model so that the live model's
// implicitly-shared containers are never detached out from under caller pointers.
class ModelSnapshotCommand : public QUndoCommand
{
public:
    ModelSnapshotCommand(DocumentController *controller, QJsonObject before, QJsonObject after,
                         const QString &text);

    void undo() override;
    void redo() override;

private:
    DocumentController *m_controller = nullptr;
    QJsonObject m_before;
    QJsonObject m_after;

    // The mutation has already been applied by the controller before push(), so the
    // first redo() (which QUndoStack always invokes) must not apply it again.
    bool m_skipNextRedo = true;
};

} // namespace bb
