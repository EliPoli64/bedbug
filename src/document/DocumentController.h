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

signals:
    void modelChanged();

private:
    Id m_activeDiagramId;
    Model m_model;
};

} // namespace bb