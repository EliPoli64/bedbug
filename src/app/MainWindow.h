#pragma once

#include "DiagramView.h"

#include <QMainWindow>

namespace bb {

class DiagramScene;
class DocumentController;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void createActions();
    void createToolBars();
    void createMenus();
    void updateStatusMessage();

    DocumentController *m_document = nullptr;
    DiagramScene *m_scene = nullptr;
    DiagramView *m_view = nullptr;
    QAction *m_addTableAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_fitAction = nullptr;
    QAction *m_snapAction = nullptr;
};

} // namespace bb