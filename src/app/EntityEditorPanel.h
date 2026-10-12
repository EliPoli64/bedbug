#pragma once

#include "Id.h"

#include <QWidget>

class QLineEdit;
class QListWidget;
class QPushButton;

namespace bb {

class DocumentController;

// Docked editor for the selected table: rename it, list its columns, and add/edit/remove
// columns through the DocumentController (PLAN.md §3 / EntityEditorPanel).
class EntityEditorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EntityEditorPanel(QWidget *parent = nullptr);

    // The panel writes through the controller; wires modelChanged -> refresh.
    void setDocument(DocumentController *controller);

    void showEntity(const Id &entityId);
    void clear();

    // Test/refresh helpers.
    int columnCount() const;
    Id columnIdAt(int row) const;
    Id entityId() const { return m_entityId; }

private:
    void refresh();
    void refreshList();
    void refreshName();
    void onNameEdited();
    void addColumn();
    void editColumn();
    void removeColumn();

    DocumentController *m_controller = nullptr;
    Id m_entityId;

    QLineEdit *m_nameEdit = nullptr;
    QListWidget *m_columnList = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_removeButton = nullptr;
};

} // namespace bb