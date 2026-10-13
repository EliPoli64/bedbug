#include "EntityEditorPanel.h"

#include "ColumnEditorDialog.h"
#include "DocumentController.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace bb {

EntityEditorPanel::EntityEditorPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *title = new QLabel(tr("Table"), this);
    title->setObjectName(QStringLiteral("panelTitle"));

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("Table name"));
    connect(m_nameEdit, &QLineEdit::editingFinished, this, &EntityEditorPanel::onNameEdited);

    m_columnList = new QListWidget(this);
    m_columnList->setObjectName(QStringLiteral("columnList"));
    m_columnList->setAlternatingRowColors(true);
    connect(m_columnList, &QListWidget::itemDoubleClicked, this,
            [this] { editColumn(); });

    m_addButton = new QPushButton(tr("Add Column"), this);
    m_editButton = new QPushButton(tr("Edit…"), this);
    m_removeButton = new QPushButton(tr("Remove"), this);
    m_addButton->setObjectName(QStringLiteral("addButton"));
    m_editButton->setObjectName(QStringLiteral("editButton"));
    m_removeButton->setObjectName(QStringLiteral("removeButton"));
    connect(m_addButton, &QPushButton::clicked, this, &EntityEditorPanel::addColumn);
    connect(m_editButton, &QPushButton::clicked, this, &EntityEditorPanel::editColumn);
    connect(m_removeButton, &QPushButton::clicked, this, &EntityEditorPanel::removeColumn);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_addButton);
    buttons->addWidget(m_editButton);
    buttons->addWidget(m_removeButton);
    buttons->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(m_nameEdit);
    layout->addWidget(m_columnList, 1);
    layout->addLayout(buttons);

    setEnabled(false);
}

void EntityEditorPanel::setDocument(DocumentController *controller)
{
    m_controller = controller;
    if (m_controller)
        connect(m_controller, &DocumentController::modelChanged, this,
                &EntityEditorPanel::refresh);
}

void EntityEditorPanel::showEntity(const Id &entityId)
{
    m_entityId = entityId;
    refresh();
}

void EntityEditorPanel::clear()
{
    m_entityId = Id();
    refresh();
}

void EntityEditorPanel::refresh()
{
    const Entity *entity = m_controller ? m_controller->model()->findEntity(m_entityId)
                                        : nullptr;
    setEnabled(entity != nullptr);
    if (!entity) {
        m_nameEdit->blockSignals(true);
        m_nameEdit->clear();
        m_nameEdit->blockSignals(false);
        m_columnList->clear();
        return;
    }
    refreshName();
    refreshList();
}

void EntityEditorPanel::refreshName()
{
    const Entity *entity = m_controller->model()->findEntity(m_entityId);
    if (!entity)
        return;
    m_nameEdit->blockSignals(true);
    m_nameEdit->setText(entity->name);
    m_nameEdit->blockSignals(false);
}

void EntityEditorPanel::refreshList()
{
    // Remember the armed column before rebuilding.
    Id previous;
    if (const QListWidgetItem *item = m_columnList->currentItem())
        previous = columnIdAt(m_columnList->row(item));

    m_columnList->clear();
    const Entity *entity = m_controller ? m_controller->model()->findEntity(m_entityId) : nullptr;
    if (!entity)
        return;

    for (int i = 0; i < entity->columns.size(); ++i) {
        const Column &column = entity->columns.at(i);
        QStringList flags;
        if (column.primaryKey)
            flags << QStringLiteral("PK");
        if (column.unique)
            flags << QStringLiteral("U");
        if (column.autoIncrement)
            flags << QStringLiteral("AI");
        if (!column.nullable)
            flags << QStringLiteral("NN");

        const QString text = QStringLiteral("%1  %2  %3")
                                 .arg(column.name, column.type.displayName(),
                                      flags.join(QStringLiteral(" ")));
        auto *item = new QListWidgetItem(text, m_columnList);
        item->setData(Qt::UserRole, column.id.toString());
        m_columnList->addItem(item);
    }

    // Re-arm the previously selected column, else the first one, so that Edit/Remove
    // always have a target even right after a model refresh wiped the selection.
    int row = -1;
    for (int i = 0; i < m_columnList->count(); ++i) {
        if (columnIdAt(i) == previous) {
            row = i;
            break;
        }
    }
    if (row < 0 && m_columnList->count() > 0)
        row = 0;
    if (row >= 0)
        m_columnList->setCurrentRow(row);
}

int EntityEditorPanel::columnCount() const
{
    return m_columnList->count();
}

Id EntityEditorPanel::columnIdAt(int row) const
{
    if (row < 0 || row >= m_columnList->count())
        return Id();
    bool ok = false;
    const Id id = Id::fromString(m_columnList->item(row)->data(Qt::UserRole).toString(), &ok);
    return ok ? id : Id();
}

void EntityEditorPanel::onNameEdited()
{
    if (!m_controller || m_entityId.isNull())
        return;
    const Entity *entity = m_controller->model()->findEntity(m_entityId);
    if (!entity)
        return;

    if (!m_controller->renameEntity(m_entityId, m_nameEdit->text())) {
        QMessageBox::warning(this, tr("Rename"),
                             tr("Rename failed: the name is empty or already in use."));
        refreshName();
    }
}

void EntityEditorPanel::addColumn()
{
    if (!m_controller || m_entityId.isNull())
        return;

    const Entity *entity = m_controller->model()->findEntity(m_entityId);
    if (!entity)
        return;

    QStringList existingNames;
    for (const Column &column : entity->columns)
        existingNames.append(column.name);

    ColumnEditorDialog dialog(this);
    dialog.setExistingNames(existingNames);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // Read the values AFTER the dialog closes — the pre-exec state is the default.
    const Column built = dialog.column();
    if (built.name.isEmpty())
        return;

    Column *added = m_controller->addColumn(m_entityId, built.name, built.type, built.nullable,
                                            built.primaryKey, built.unique, built.autoIncrement,
                                            built.defaultValue, built.comment);
    if (added) {
        // Arm the newly added column in the list.
        for (int i = 0; i < m_columnList->count(); ++i) {
            if (columnIdAt(i) == added->id) {
                m_columnList->setCurrentRow(i);
                break;
            }
        }
    }
}

void EntityEditorPanel::editColumn()
{
    if (!m_controller || m_entityId.isNull())
        return;
    const QListWidgetItem *selected = m_columnList->currentItem();
    if (!selected)
        return;

    const Id columnId = columnIdAt(m_columnList->row(selected));
    const Entity *entity = m_controller->model()->findEntity(m_entityId);
    const Column *existing = entity ? entity->findColumn(columnId) : nullptr;
    if (!existing)
        return;

    QStringList otherNames;
    for (const Column &column : entity->columns) {
        if (column.id != columnId)
            otherNames.append(column.name);
    }

    ColumnEditorDialog dialog(this);
    dialog.setExistingNames(otherNames);
    dialog.setColumn(*existing);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Column updated = dialog.column();
    updated.id = existing->id;
    m_controller->updateColumn(m_entityId, updated);
}

void EntityEditorPanel::removeColumn()
{
    if (!m_controller || m_entityId.isNull())
        return;
    const QListWidgetItem *selected = m_columnList->currentItem();
    if (!selected)
        return;

    const Id columnId = columnIdAt(m_columnList->row(selected));
    const QString question =
        tr("Remove column '%1'? Associated relationships and indexes will be dropped.")
            .arg(selected->text().section(QLatin1Char(' '), 0, 0));
    if (QMessageBox::question(this, tr("Remove column"), question) == QMessageBox::Yes)
        m_controller->removeColumn(m_entityId, columnId);
}

} // namespace bb