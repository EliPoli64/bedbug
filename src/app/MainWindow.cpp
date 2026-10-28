#include "MainWindow.h"

#include "ColumnEditorDialog.h"
#include "DiagramScene.h"
#include "DocumentController.h"
#include "EntityEditorPanel.h"
#include "EntityItem.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QStandardPaths>
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
               RelationshipKind kind) -> bool {
            if (kind == RelationshipKind::ManyToMany)
                return m_document->createManyToMany(childEntityId, parentEntityId) != nullptr;

            const Id childColumn =
                childColumnIds.isEmpty() ? Id() : childColumnIds.first();
            const Id parentColumn =
                parentColumnIds.isEmpty() ? Id() : parentColumnIds.first();
            return m_document->addRelationshipPick(kind, childEntityId, childColumn, parentEntityId,
                                                   parentColumn)
                   != nullptr;
        });
    connect(m_document, &DocumentController::modelChanged, this, [this] {
        updateWindowTitle();
    });
    connect(m_scene, &QGraphicsScene::selectionChanged, this, [this] {
        statusBar()->showMessage(QString());
        m_entityPanel->showEntity(selectedEntityId(m_scene));
    });
    connect(m_scene, &DiagramScene::tableContextMenuRequested, this,
            &MainWindow::showTableContextMenu);
    connect(m_scene, &DiagramScene::canvasContextMenuRequested, this,
            &MainWindow::showCanvasContextMenu);
    connect(m_scene, &DiagramScene::canvasChanged, this, [this] {
        m_view->setSceneRect(m_scene->scrollableRect());
        updateStatusMessage();
        updateWindowTitle();
    });
    m_scene->setMarkDirtyCallback([this] {
        m_document->markDirty();
        updateWindowTitle();
    });
    createActions();
    createDock();
    createToolBars();
    createMenus();

    // Come back to the previous board, even when nothing was ever saved.
    restoreSession();
    updateWindowTitle();

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
    m_newAction = new QAction(tr("&New"), this);
    m_newAction->setShortcut(QKeySequence::New);
    connect(m_newAction, &QAction::triggered, m_document, &DocumentController::newDocument);

    m_openAction = new QAction(tr("&Open…"), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openDocument);

    m_saveAction = new QAction(tr("&Save"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveDocument);

    m_saveAsAction = new QAction(tr("Save &As…"), this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveDocumentAs);

    m_addTableAction = new QAction(tr("Add Table"), this);
    m_addTableAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    m_addTableAction->setStatusTip(tr("Create a new table on the canvas"));
    connect(m_addTableAction, &QAction::triggered, this, [this] {
        // Suggest the next free "tableN" name.
        int counter = 1;
        QString suggested;
        const auto &entities = m_document->model()->entities;
        do {
            suggested = QStringLiteral("table%1").arg(counter++);
            bool taken = false;
            for (const Entity &entity : entities) {
                if (entity.name.compare(suggested, Qt::CaseInsensitive) == 0) {
                    taken = true;
                    break;
                }
            }
            if (!taken)
                break;
        } while (true);

        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Add Table"), tr("Table name:"),
                                                   QLineEdit::Normal, suggested, &ok);
        if (!ok || name.trimmed().isEmpty())
            return;
        const QString trimmed = name.trimmed();
        for (const Entity &entity : m_document->model()->entities) {
            if (entity.name.compare(trimmed, Qt::CaseInsensitive) == 0) {
                QMessageBox::warning(this, tr("Add Table"),
                                     tr("A table named '%1' already exists.").arg(trimmed));
                return;
            }
        }
        Entity *entity = m_document->addEntity(trimmed);
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

    m_addSheetLeftAction = new QAction(tr("Add Sheet Left"), this);
    m_addSheetRightAction = new QAction(tr("Add Sheet Right"), this);
    m_addSheetUpAction = new QAction(tr("Add Sheet Above"), this);
    m_addSheetDownAction = new QAction(tr("Add Sheet Below"), this);
    m_removeSheetAction = new QAction(tr("Remove Last Sheet"), this);
    connect(m_addSheetLeftAction, &QAction::triggered, m_document, &DocumentController::addSheetLeft);
    connect(m_addSheetRightAction, &QAction::triggered, m_document, &DocumentController::addSheetRight);
    connect(m_addSheetUpAction, &QAction::triggered, m_document, &DocumentController::addSheetUp);
    connect(m_addSheetDownAction, &QAction::triggered, m_document, &DocumentController::addSheetDown);
    connect(m_removeSheetAction, &QAction::triggered, this, [this] {
        if (m_scene->pagesX() > 1)
            m_document->removeSheetRight();
        else if (m_scene->pagesY() > 1)
            m_document->removeSheetDown();
        else if (m_scene->originX() < 0)
            m_document->removeSheetLeft();
        else if (m_scene->originY() < 0)
            m_document->removeSheetUp();
    });
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
    toolBar->addSeparator();
    toolBar->addAction(m_addSheetLeftAction);
    toolBar->addAction(m_addSheetRightAction);
    toolBar->addAction(m_addSheetUpAction);
    toolBar->addAction(m_addSheetDownAction);
    toolBar->addSeparator();
    toolBar->addAction(m_removeSheetAction);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_newAction);
    fileMenu->addAction(m_openAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
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
    viewMenu->addSeparator();
    QMenu *sheetsMenu = viewMenu->addMenu(tr("Sheets"));
    sheetsMenu->addAction(m_addSheetLeftAction);
    sheetsMenu->addAction(m_addSheetRightAction);
    sheetsMenu->addAction(m_addSheetUpAction);
    sheetsMenu->addAction(m_addSheetDownAction);
    sheetsMenu->addSeparator();
    sheetsMenu->addAction(m_removeSheetAction);
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
    if (m_initialCenter) {
        m_view->centerOn(m_scene->canvasRect().center());
        m_initialCenter = false;
    }
    updateStatusMessage();
}

void MainWindow::showCanvasContextMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    QMenu *addSheetMenu = menu.addMenu(tr("Add Sheet"));
    addSheetMenu->addAction(m_addSheetLeftAction);
    addSheetMenu->addAction(m_addSheetRightAction);
    addSheetMenu->addAction(m_addSheetUpAction);
    addSheetMenu->addAction(m_addSheetDownAction);
    menu.addSeparator();
    menu.addAction(m_removeSheetAction);
    menu.exec(globalPos);
}

void MainWindow::updateStatusMessage()
{
    statusBar()->showMessage(
        tr("%1 table(s), %2 relationship(s), %3×%4 sheets")
            .arg(m_document->model()->entities.size())
            .arg(m_document->model()->relationships.size())
            .arg(m_scene->pagesX())
            .arg(m_scene->pagesY()));
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
        if (name.trimmed().isEmpty() || name == entity->name)
            return;
        if (!m_document->renameEntity(entityId, name))
            QMessageBox::warning(this, tr("Rename Table"),
                                 tr("A table named '%1' already exists.").arg(name.trimmed()));
    } else if (chosen == link) {
        focusEntity(entityId);
        m_oneToManyAction->setChecked(true);
    } else if (chosen == editColumns) {
        focusEntity(entityId);
    } else if (chosen == remove) {
        removeTable(entityId);
    }
}

void MainWindow::openDocument()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Document"), QString(),
        tr("bedbug documents (*.json);;All files (*)"));
    if (path.isEmpty())
        return;

    QString error;
    if (!m_document->loadFromFile(path, &error))
        QMessageBox::warning(this, tr("Open Document"),
                             tr("Could not open '%1':\n%2").arg(path, error));
}

void MainWindow::saveDocument()
{
    if (!m_document->hasFile()) {
        saveDocumentAs();
        return;
    }
    QString error;
    if (!m_document->saveToFile(m_document->filePath(), &error))
        QMessageBox::warning(this, tr("Save Document"),
                             tr("Could not save '%1':\n%2").arg(m_document->filePath(), error));
}

void MainWindow::saveDocumentAs()
{
    const QString suggested = m_document->model()->name.isEmpty()
                                  ? QStringLiteral("untitled.json")
                                  : m_document->model()->name + QStringLiteral(".json");
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Document As"), suggested,
                                                      tr("bedbug documents (*.json)"));
    if (path.isEmpty())
        return;

    QString error;
    if (!m_document->saveToFile(path, &error))
        QMessageBox::warning(this, tr("Save Document"), tr("Could not save:\n%1").arg(error));
}

void MainWindow::updateWindowTitle()
{
    QString documentName;
    if (m_document->hasFile())
        documentName = QFileInfo(m_document->filePath()).completeBaseName();
    else
        documentName = m_document->model()->name;

    setWindowTitle(tr("bedbug — %1[*]")
                       .arg(documentName.isEmpty() ? tr("Untitled") : documentName));
    setWindowModified(m_document->isModified());
}

QString MainWindow::sessionFilePath() const
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(directory);
    return directory + QStringLiteral("/session.bedbug.json");
}

void MainWindow::restoreSession()
{
    const QString path = sessionFilePath();
    if (!QFileInfo::exists(path))
        return;
    QString error;
    if (!m_document->loadSession(path, &error))
        qWarning() << "Could not restore session:" << error;
}

void MainWindow::saveSession()
{
    QString error;
    if (!m_document->saveSession(sessionFilePath(), &error))
        qWarning() << "Could not save session:" << error;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSession();
    QMainWindow::closeEvent(event);
}

} // namespace bb