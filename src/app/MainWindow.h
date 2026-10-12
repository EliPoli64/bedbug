#pragma once

#include "DiagramView.h"
#include "Id.h"

#include <QMainWindow>

namespace bb {

class DiagramScene;
class DocumentController;
class EntityEditorPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void createActions();
    void createToolBars();
    void createMenus();
    void createDock();
    void syncScenePreservingSelection();
    void updateStatusMessage();

    void focusEntity(const Id &entityId);
    void addColumnToEntity(const Id &entityId);
    void removeTable(const Id &entityId);
    void showTableContextMenu(const Id &entityId, const QPoint &globalPos);

    DocumentController *m_document = nullptr;
    DiagramScene *m_scene = nullptr;
    DiagramView *m_view = nullptr;
    EntityEditorPanel *m_entityPanel = nullptr;
    QAction *m_addTableAction = nullptr;
    QAction *m_linkAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_fitAction = nullptr;
    QAction *m_snapAction = nullptr;
    Id m_pendingSelect;
};

} // namespace bb