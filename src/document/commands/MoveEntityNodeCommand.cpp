#include "MoveEntityNodeCommand.h"

#include "DocumentController.h"

#include <QObject>

namespace bb {

MoveEntityNodeCommand::MoveEntityNodeCommand(DocumentController *controller,
                                             const Id &diagramId, const Id &entityId,
                                             const QPointF &before, const QPointF &after,
                                             int mergeId)
    : QUndoCommand(QObject::tr("Move table"))
    , m_controller(controller)
    , m_diagramId(diagramId)
    , m_entityId(entityId)
    , m_before(before)
    , m_after(after)
    , m_mergeId(mergeId)
{
}

void MoveEntityNodeCommand::undo()
{
    if (m_controller)
        m_controller->applyNodePosition(m_diagramId, m_entityId, m_before);
}

void MoveEntityNodeCommand::redo()
{
    if (m_skipNextRedo) {
        m_skipNextRedo = false;
        return;
    }
    if (m_controller)
        m_controller->applyNodePosition(m_diagramId, m_entityId, m_after);
}

int MoveEntityNodeCommand::id() const
{
    return m_mergeId;
}

bool MoveEntityNodeCommand::mergeWith(const QUndoCommand *other)
{
    const auto *next = dynamic_cast<const MoveEntityNodeCommand *>(other);
    if (!next || m_mergeId < 0 || next->m_mergeId != m_mergeId)
        return false;

    // Keep the original starting point and adopt the newest position.
    m_after = next->m_after;
    return true;
}

} // namespace bb
