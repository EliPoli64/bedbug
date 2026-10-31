#include "ModelSnapshotCommand.h"

#include "DocumentController.h"

namespace bb {

ModelSnapshotCommand::ModelSnapshotCommand(DocumentController *controller, QJsonObject before,
                                           QJsonObject after, const QString &text)
    : QUndoCommand(text)
    , m_controller(controller)
    , m_before(std::move(before))
    , m_after(std::move(after))
{
}

void ModelSnapshotCommand::undo()
{
    if (m_controller)
        m_controller->applySnapshot(m_before);
}

void ModelSnapshotCommand::redo()
{
    if (m_skipNextRedo) {
        m_skipNextRedo = false;
        return;
    }
    if (m_controller)
        m_controller->applySnapshot(m_after);
}

} // namespace bb
