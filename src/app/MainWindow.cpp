#include "MainWindow.h"

#include "DiagramScene.h"
#include "DocumentController.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QShortcut>
#include <QStatusBar>
#include <QToolBar>

namespace bb {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_document(new DocumentController(this))
    , m_scene(new DiagramScene(this))
    , m_view(new DiagramView(m_scene, this))
{
    setWindowTitle(QStringLiteral("bedbug"));
    resize(1200, 800);

    setCentralWidget(m_view);

    m_scene->setModel(m_document->model());
    m_scene->setActiveDiagramId(m_document->activeDiagramId());
    connect(m_document, &DocumentController::modelChanged, this, [this] {
        m_scene->syncFromModel();
        updateStatusMessage();
    });
    connect(m_scene, &DiagramScene::entityMoved, this, [this] {
        updateStatusMessage();
    });
    connect(m_scene, &DiagramScene::linkStatusChanged, this, [this](const QString &text) {
        statusBar()->showMessage(text);
    });
    m_scene->setRelationshipCreationHandler([this](const Id &childEntityId,
                                                   const QList<Id> &childColumnIds,
                                                   const Id &parentEntityId,
                                                   const QList<Id> &parentColumnIds) {
        m_document->addRelationship(childEntityId, childColumnIds, parentEntityId,
                                    parentColumnIds);
    });

    createActions();
    createToolBars();
    createMenus();

    auto *escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escShortcut, &QShortcut::activated, this, [this] {
        if (m_linkAction->isChecked())
            m_linkAction->setChecked(false);
    });

    updateStatusMessage();
}

void MainWindow::createActions()
{
    m_addTableAction = new QAction(tr("Add Table"), this);
    m_addTableAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    m_addTableAction->setStatusTip(tr("Create a new table on the canvas"));
    connect(m_addTableAction, &QAction::triggered, this, [this] {
        m_document->addEntity(QStringLiteral("table"));
    });

    m_linkAction = new QAction(tr("Link Mode"), this);
    m_linkAction->setCheckable(true);
    m_linkAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(m_linkAction, &QAction::toggled, m_scene, &DiagramScene::setLinkMode);

    m_zoomInAction = new QAction(tr("Zoom In"), this);
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, m_view, &DiagramView::zoomIn);

    m_zoomOutAction = new QAction(tr("Zoom Out"), this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, m_view, &DiagramView::zoomOut);

    m_fitAction = new QAction(tr("Fit to Window"), this);
    m_fitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_fitAction, &QAction::triggered, m_view, &DiagramView::fitToContent);

    m_snapAction = new QAction(tr("Snap to Grid"), this);
    m_snapAction->setCheckable(true);
    m_snapAction->setChecked(true);
    connect(m_snapAction, &QAction::toggled, m_scene, &DiagramScene::setSnapToGrid);
}

void MainWindow::createToolBars()
{
    QToolBar *toolBar = addToolBar(tr("Main"));
    toolBar->setObjectName(QStringLiteral("mainToolBar"));
    toolBar->setMovable(false);
    toolBar->addAction(m_addTableAction);
    toolBar->addAction(m_linkAction);
    toolBar->addSeparator();
    toolBar->addAction(m_zoomInAction);
    toolBar->addAction(m_zoomOutAction);
    toolBar->addAction(m_fitAction);
    toolBar->addSeparator();
    toolBar->addAction(m_snapAction);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&New"), QKeySequence::New, this, &MainWindow::close);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), QKeySequence::Quit, this, &MainWindow::close);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(m_addTableAction);
    editMenu->addAction(m_linkAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_fitAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_snapAction);
}

void MainWindow::updateStatusMessage()
{
    const int entityCount = m_document->model()->entities.size();
    statusBar()->showMessage(tr("%1 table(s), %2 relationship(s)")
                                 .arg(entityCount)
                                 .arg(m_document->model()->relationships.size()));
}

} // namespace bb