#pragma once

#include "Id.h"

#include <QPointF>
#include <QUndoCommand>

namespace bb {

class DocumentController;

// Moves one table node inside a diagram. Consecutive moves of the same node merge, so a
// whole drag becomes a single undo step.
class MoveEntityNodeCommand : public QUndoCommand
{
public:
    MoveEntityNodeCommand(DocumentController *controller, const Id &diagramId,
                          const Id &entityId, const QPointF &before, const QPointF &after,
                          int mergeId);

    void undo() override;
    void redo() override;

    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

private:
    DocumentController *m_controller = nullptr;
    Id m_diagramId;
    Id m_entityId;
    QPointF m_before;
    QPointF m_after;
    int m_mergeId = -1;
    bool m_skipNextRedo = true;
};

} // namespace bb
