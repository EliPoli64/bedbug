#include "MainWindow.h"

#include "ColumnEditorDialog.h"
#include "DiagramScene.h"
#include "DocumentController.h"
#include "EntityEditorPanel.h"
#include "EntityItem.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDockWidget>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QStatusBar>
#include <QToolBar>

#include <optional>

namespace bb {

namespace {

Id selectedEntityId(const DiagramScene *scene)
{
    for (QGraphicsItem *item : scene->selectedItems()) {
        if (auto *entityItem = dynamic_cast<EntityItem *>(item))
            return entityItem->entityId();
    }
    return Id();
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_document(new DocumentController(this))
    , m_scene(new DiagramScene(this))
    , m_view(new DiagramView(m_scene, this))
{
    setWindowTitle(QStringLiteral("bedbug"));
    resize(1280, 820);

    setCentralWidget(m_view);

    m_scene->setModel(m_document->model());
    m_scene->setActiveDiagramId(m_document->activeDiagramId());
    connect(m_document, &DocumentController::modelChanged, this,
            &MainWindow::syncScenePreservingSelection);
    connect(m_scene, &DiagramScene::entityMoved, this, [this] { updateStatusMessage(); });
    connect(m_scene, &DiagramScene::linkStatusChanged, this, [this](const QString &text) {
        statusBar()->showMessage(text);
    });
    m_scene->setRelationshipCreationHandler(
        [this](const Id &childEntityId, const QList<Id> &childColumnIds,
               const Id &parentEntityId, const QList<Id> &parentColumnIds,
               RelationshipKind kind) {
            if (kind == RelationshipKind::ManyToMany) {
                m_document->createManyToMany(childEntityId, parentEntityId);
            } else {
                m_document->addRelationship(kind, childEntityId, childColumnIds, parentEntityId,
                                            parentColumnIds);
            }
        });
    connect(m_scene, &QGraphicsScene::selectionChanged, this, [this] {
        statusBar()->showMessage(QString());
        m_entityPanel->showEntity(selectedEntityId(m_scene));
    });
    connect(m_scene, &DiagramScene::tableContextMenuRequested, this,
            &MainWindow::showTableContextMenu);

    createActions();
    createDock();
    createToolBars();
    createMenus();

    auto *escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escShortcut, &QShortcut::activated, this, [this] {
        for (QAction *action : {m_oneToOneAction, m_oneToManyAction, m_manyToManyAction}) {
            if (action->isChecked())
                action->setChecked(false);
        }
    });

    updateStatusMessage();
}

void MainWindow::createActions()
{
    m_addTableAction = new QAction(tr("Add Table"), this);
    m_addTableAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    m_addTableAction->setStatusTip(tr("Create a new table on the canvas"));
    connect(m_addTableAction, &QAction::triggered, this, [this] {
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Add Table"), tr("Table name:"),
                                                   QLineEdit::Normal, QStringLiteral("table"), &ok);
        if (!ok || name.trimmed().isEmpty())
            return;
        Entity *entity = m_document->addEntity(name.trimmed());
        m_pendingSelect = entity ? entity->id : Id();
    });

    // Relationship tools (MySQL Workbench style): 1:1, 1:N, N:M. One is active at a time.
    auto makeRelationshipTool = [this](const QString &label, RelationshipKind kind,
                                       const QKeySequence &shortcut = QKeySequence()) {
        auto *action = new QAction(label, this);
        action->setCheckable(true);
        action->setShortcut(shortcut);
        connect(action, &QAction::toggled, this, [this, kind](bool checked) {
            m_scene->setLinkTool(checked ? std::make_optional(kind) : std::nullopt);
        });
        return action;
    };
    m_oneToOneAction = makeRelationshipTool(tr("1:1"), RelationshipKind::OneToOne);
    m_oneToManyAction =
        makeRelationshipTool(tr("1:N"), RelationshipKind::OneToMany, QKeySequence(Qt::CTRL | Qt::Key_L));
    m_manyToManyAction = makeRelationshipTool(tr("N:M"), RelationshipKind::ManyToMany);

    auto *relationshipGroup = new QActionGroup(this);
    relationshipGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    relationshipGroup->addAction(m_oneToOneAction);
    relationshipGroup->addAction(m_oneToManyAction);
    relationshipGroup->addAction(m_manyToManyAction);

    m_pointedAction = new QAction(tr("Pointed Lines"), this);
    m_pointedAction->setCheckable(true);
    m_pointedAction->setStatusTip(
        tr("Draw a direction arrow at the parent end of relationship edges"));
    connect(m_pointedAction, &QAction::toggled, m_scene, &DiagramScene::setPointedLines);

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
    m_snapAction->setChecked(false);   // free dragging by default
    connect(m_snapAction, &QAction::toggled, m_scene, &DiagramScene::setSnapToGrid);
}

void MainWindow::createToolBars()
{
    QToolBar *toolBar = addToolBar(tr("Main"));
    toolBar->setObjectName(QStringLiteral("mainToolBar"));
    toolBar->setMovable(false);
    toolBar->addAction(m_addTableAction);
    toolBar->addSeparator();
    toolBar->addAction(m_oneToOneAction);
    toolBar->addAction(m_oneToManyAction);
    toolBar->addAction(m_manyToManyAction);
    toolBar->addSeparator();
    toolBar->addAction(m_pointedAction);
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
    editMenu->addSeparator();
    editMenu->addAction(m_oneToOneAction);
    editMenu->addAction(m_oneToManyAction);
    editMenu->addAction(m_manyToManyAction);
    editMenu->addSeparator();
    editMenu->addAction(m_pointedAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_fitAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_snapAction);
    if (auto *dock = qobject_cast<QDockWidget *>(m_entityPanel->parentWidget()))
        viewMenu->addAction(dock->toggleViewAction());
}

void MainWindow::createDock()
{
    auto *dock = new QDockWidget(tr("Entity Editor"), this);
    dock->setObjectName(QStringLiteral("entityEditorDock"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_entityPanel = new EntityEditorPanel(dock);
    m_entityPanel->setDocument(m_document);
    dock->setWidget(m_entityPanel);

    addDockWidget(Qt::RightDockWidgetArea, dock);
}

void MainWindow::syncScenePreservingSelection()
{
    const Id keep = !m_pendingSelect.isNull() ? m_pendingSelect : selectedEntityId(m_scene);
    m_pendingSelect = Id();

    m_scene->syncFromModel();
    if (!keep.isNull()) {
        if (EntityItem *item = m_scene->itemForEntity(keep))
            item->setSelected(true);
    }
    updateStatusMessage();
}

void MainWindow::updateStatusMessage()
{
    statusBar()->showMessage(tr("%1 table(s), %2 relationship(s)")
                                 .arg(m_document->model()->entities.size())
                                 .arg(m_document->model()->relationships.size()));
}

void MainWindow::focusEntity(const Id &entityId)
{
    if (entityId.isNull())
        return;
    if (EntityItem *item = m_scene->itemForEntity(entityId))
        item->setSelected(true);
    m_entityPanel->showEntity(entityId);
}

void MainWindow::addColumnToEntity(const Id &entityId)
{
    Entity *entity = m_document->model()->findEntity(entityId);
    if (!entity)
        return;

    // The table stays focused while the dialog is open so the panel follows.
    focusEntity(entityId);

    QStringList existingNames;
    for (const Column &column : entity->columns)
        existingNames.append(column.name);

    ColumnEditorDialog dialog(this);
    dialog.setExistingNames(existingNames);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const Column built = dialog.column();
    if (built.name.isEmpty())
        return;

    m_document->addColumn(entityId, built.name, built.type, built.nullable, built.primaryKey,
                          built.unique, built.autoIncrement, built.defaultValue, built.comment);
}

void MainWindow::removeTable(const Id &entityId)
{
    const Entity *entity = m_document->model()->findEntity(entityId);
    if (!entity)
        return;

    const int relationshipCount =
        m_document->model()->relationshipsFor(entityId).size();
    const QString question =
        relationshipCount > 0
            ? tr("Remove table '%1' and its %n relationship(s)?", nullptr, relationshipCount)
                  .arg(entity->name)
            : tr("Remove table '%1'?").arg(entity->name);

    if (QMessageBox::question(this, tr("Remove Table"), question) == QMessageBox::Yes)
        m_document->removeEntity(entityId);
}

void MainWindow::showTableContextMenu(const Id &entityId, const QPoint &globalPos)
{
    const Entity *entity = m_document->model()->findEntity(entityId);
    if (!entity)
        return;

    QMenu menu(this);
    menu.addSection(entity->name.isEmpty() ? tr("Table") : entity->name);

    QAction *addColumn = menu.addAction(tr("Add Column…"));
    QAction *editColumns = menu.addAction(tr("Edit Columns"));
    QAction *rename = menu.addAction(tr("Rename…"));
    QAction *link = menu.addAction(tr("Link Relationship"));
    menu.addSeparator();
    QAction *remove = menu.addAction(tr("Remove Table…"));

    QAction *chosen = menu.exec(globalPos);
    if (chosen == addColumn) {
        addColumnToEntity(entityId);
    } else if (chosen == rename) {
        const QString name = QInputDialog::getText(this, tr("Rename Table"), tr("Table name:"),
                                                   QLineEdit::Normal, entity->name);
        if (!name.trimmed().isEmpty() && name != entity->name)
            m_document->renameEntity(entityId, name);
    } else if (chosen == link) {
        focusEntity(entityId);
        m_oneToManyAction->setChecked(true);
    } else if (chosen == editColumns) {
        focusEntity(entityId);
    } else if (chosen == remove) {
        removeTable(entityId);
    }
}

} // namespace bb