#pragma once

#include "CanonicalType.h"
#include "Column.h"

#include <QDialog>

class QComboBox;
class QFormLayout;
class QLineEdit;
class QSpinBox;
class QCheckBox;

namespace bb {

// Form for editing one column definition (name, canonical type + parameters,
// flags, default value, comment). The returned Column carries a null id unless the
// caller supplies one; the caller owns id assignment.
class ColumnEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ColumnEditorDialog(QWidget *parent = nullptr);

    void setColumn(const Column &column);
    Column column() const;

    // Names that must not be reused (the entity's other columns). Used by accept().
    void setExistingNames(const QStringList &names);

    // Returns a non-empty message when the current name is invalid for the given
    // existing names (empty or duplicate, case-insensitive).
    QString validationError(const QStringList &existingNames) const;

public slots:
    void accept() override;

private:
    void updateParameterEdits();
    Column build() const;

    QStringList m_existingNames;
    QFormLayout *m_form = nullptr;
    int m_enumRow = -1;

    QLineEdit *m_name = nullptr;
    QComboBox *m_kind = nullptr;
    QSpinBox *m_length = nullptr;
    QSpinBox *m_precision = nullptr;
    QSpinBox *m_scale = nullptr;
    QLineEdit *m_enumValues = nullptr;
    QCheckBox *m_nullable = nullptr;
    QCheckBox *m_primaryKey = nullptr;
    QCheckBox *m_unique = nullptr;
    QCheckBox *m_autoIncrement = nullptr;
    QLineEdit *m_defaultValue = nullptr;
    QLineEdit *m_comment = nullptr;
};

} // namespace bb