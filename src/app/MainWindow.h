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
    ~MainWindow() override;

    // Debug/test hook.
    void debugAddTable(const QString &name = QStringLiteral("table2"));

private:
    void createActions();
    void createToolBars();
    void createMenus();
    void createDock();
    void syncScenePreservingSelection();
    void updateStatusMessage();

    bool m_initialCenter = true;
    bool m_addingTable = false;

    void focusEntity(const Id &entityId);
    void addColumnToEntity(const Id &entityId);
    void removeTable(const Id &entityId);
    void showTableContextMenu(const Id &entityId, const QPoint &globalPos);
    void showCanvasContextMenu(const QPoint &globalPos);

    // File actions.
    void openDocument();
    void saveDocument();
    void saveDocumentAs();
    void updateWindowTitle();

    // Session autosave: the board is persisted on close and restored on startup so the
    // app comes back to where you left off, even without an explicit save.
    QString sessionFilePath() const;
    void restoreSession();
    void saveSession();

protected:
    void closeEvent(QCloseEvent *event) override;

    DocumentController *m_document = nullptr;
    DiagramScene *m_scene = nullptr;
    DiagramView *m_view = nullptr;
    EntityEditorPanel *m_entityPanel = nullptr;
    QAction *m_addTableAction = nullptr;
    QAction *m_newAction = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_saveAsAction = nullptr;
    QAction *m_oneToOneAction = nullptr;
    QAction *m_oneToManyAction = nullptr;
    QAction *m_manyToManyAction = nullptr;
    QAction *m_pointedAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_fitAction = nullptr;
    QAction *m_snapAction = nullptr;
    QAction *m_addSheetLeftAction = nullptr;
    QAction *m_addSheetRightAction = nullptr;
    QAction *m_addSheetUpAction = nullptr;
    QAction *m_addSheetDownAction = nullptr;
    QAction *m_removeSheetAction = nullptr;
    Id m_pendingSelect;
};

} // namespace bb