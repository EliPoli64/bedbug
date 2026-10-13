#include "ColumnEditorDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <array>
#include <utility>

namespace bb {

namespace {

QStringList kindNames()
{
    QStringList names;
    const std::array kinds = {CanonicalType::Kind::Int,
                              CanonicalType::Kind::BigInt,
                              CanonicalType::Kind::SmallInt,
                              CanonicalType::Kind::Varchar,
                              CanonicalType::Kind::Char,
                              CanonicalType::Kind::Text,
                              CanonicalType::Kind::Boolean,
                              CanonicalType::Kind::Date,
                              CanonicalType::Kind::Time,
                              CanonicalType::Kind::Timestamp,
                              CanonicalType::Kind::Decimal,
                              CanonicalType::Kind::Float,
                              CanonicalType::Kind::Double,
                              CanonicalType::Kind::Blob,
                              CanonicalType::Kind::Json,
                              CanonicalType::Kind::Uuid,
                              CanonicalType::Kind::Enum};
    for (const CanonicalType::Kind kind : kinds)
        names.append(CanonicalType::kindToString(kind));
    return names;
}

} // namespace

ColumnEditorDialog::ColumnEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Column"));

    m_name = new QLineEdit(this);
    m_name->setObjectName(QStringLiteral("nameEdit"));
    m_name->setPlaceholderText(QStringLiteral("Column name"));

    m_kind = new QComboBox(this);
    m_kind->setObjectName(QStringLiteral("kindCombo"));
    m_kind->addItems(kindNames());

    m_length = new QSpinBox(this);
    m_length->setObjectName(QStringLiteral("lengthSpin"));
    m_length->setRange(0, 65535);
    m_length->setValue(255);

    m_precision = new QSpinBox(this);
    m_precision->setObjectName(QStringLiteral("precisionSpin"));
    m_precision->setRange(0, 39);
    m_precision->setValue(10);

    m_scale = new QSpinBox(this);
    m_scale->setObjectName(QStringLiteral("scaleSpin"));
    m_scale->setRange(0, 10);

    m_enumValues = new QLineEdit(this);
    m_enumValues->setObjectName(QStringLiteral("enumValuesEdit"));
    m_enumValues->setPlaceholderText(QStringLiteral("a, b, c"));

    m_nullable = new QCheckBox(QStringLiteral("Nullable"), this);
    m_nullable->setObjectName(QStringLiteral("nullableCheck"));
    m_primaryKey = new QCheckBox(QStringLiteral("Primary key"), this);
    m_primaryKey->setObjectName(QStringLiteral("primaryKeyCheck"));
    m_unique = new QCheckBox(QStringLiteral("Unique"), this);
    m_unique->setObjectName(QStringLiteral("uniqueCheck"));
    m_autoIncrement = new QCheckBox(QStringLiteral("Auto increment"), this);
    m_autoIncrement->setObjectName(QStringLiteral("autoIncrementCheck"));

    m_defaultValue = new QLineEdit(this);
    m_defaultValue->setObjectName(QStringLiteral("defaultValueEdit"));
    m_defaultValue->setPlaceholderText(QStringLiteral("e.g. CURRENT_TIMESTAMP"));
    m_comment = new QLineEdit(this);
    m_comment->setObjectName(QStringLiteral("commentEdit"));

    m_form = new QFormLayout;
    m_form->addRow(QStringLiteral("Name"), m_name);
    m_form->addRow(QStringLiteral("Type"), m_kind);
    m_form->addRow(QStringLiteral("Length"), m_length);
    m_form->addRow(QStringLiteral("Precision"), m_precision);
    m_form->addRow(QStringLiteral("Scale"), m_scale);
    m_form->addRow(QStringLiteral("Enum values"), m_enumValues);
    m_form->addRow(QStringLiteral("Default"), m_defaultValue);
    m_form->addRow(QStringLiteral("Comment"), m_comment);
    m_form->addRow(QString(), m_nullable);
    m_form->addRow(QString(), m_primaryKey);
    m_form->addRow(QString(), m_unique);
    m_form->addRow(QString(), m_autoIncrement);
    // Rows are added in fixed order above; the enums row is the 6th (index 5).
    m_enumRow = 5;

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(m_form);
    layout->addWidget(buttons);

    connect(m_kind, &QComboBox::currentIndexChanged, this,
            [this] { updateParameterEdits(); });

    // Selecting "Primary key" implies auto-increment, unique, and not-null.
    connect(m_primaryKey, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            m_autoIncrement->setChecked(true);
            m_unique->setChecked(true);
            m_nullable->setChecked(false);
        }
    });

    // Defaults for a new column.
    m_nullable->setChecked(true);
    updateParameterEdits();
}

void ColumnEditorDialog::setColumn(const Column &column)
{
    m_name->setText(column.name);
    const int kindIndex = m_kind->findText(CanonicalType::kindToString(column.type.kind));
    m_kind->setCurrentIndex(qMax(0, kindIndex));
    m_length->setValue(column.type.length > 0 ? column.type.length : 255);
    m_precision->setValue(column.type.precision > 0 ? column.type.precision : 10);
    m_scale->setValue(column.type.scale);
    QStringList values;
    for (const QString &value : column.type.enumValues)
        values.append(value);
    m_enumValues->setText(values.join(QStringLiteral(", ")));
    m_nullable->setChecked(column.nullable);
    m_primaryKey->setChecked(column.primaryKey);
    m_unique->setChecked(column.unique);
    m_autoIncrement->setChecked(column.autoIncrement);
    m_defaultValue->setText(column.defaultValue);
    m_comment->setText(column.comment);
    updateParameterEdits();
}

void ColumnEditorDialog::setExistingNames(const QStringList &names)
{
    m_existingNames = names;
}

QString ColumnEditorDialog::validationError(const QStringList &existingNames) const
{
    const QString name = m_name->text().trimmed();
    if (name.isEmpty())
        return QStringLiteral("Column name cannot be empty.");
    for (const QString &existing : existingNames) {
        if (existing.compare(name, Qt::CaseInsensitive) == 0)
            return QStringLiteral("A column named '%1' already exists.").arg(name);
    }
    return {};
}

void ColumnEditorDialog::accept()
{
    const QString error = validationError(m_existingNames);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Column"), error);
        return;
    }
    QDialog::accept();
}

void ColumnEditorDialog::updateParameterEdits()
{
    using K = CanonicalType::Kind;
    const K kind = CanonicalType::stringToKind(m_kind->currentText());
    const bool sized = kind == K::Varchar || kind == K::Char;
    const bool decimal = kind == K::Decimal;
    const bool enumerable = kind == K::Enum;

    m_length->setEnabled(sized);
    m_precision->setEnabled(decimal);
    m_scale->setEnabled(decimal);

    // The enum-values field only makes sense for enum types.
    if (m_form && m_enumRow >= 0)
        m_form->setRowVisible(m_enumRow, enumerable);
    m_enumValues->setEnabled(enumerable);
}

Column ColumnEditorDialog::build() const
{
    using K = CanonicalType::Kind;
    Column column;
    column.name = m_name->text().trimmed();
    bool kindOk = false;
    column.type.kind = CanonicalType::stringToKind(m_kind->currentText(), &kindOk);
    if (!kindOk)
        column.type.kind = K::Varchar;
    column.type.length = m_length->value();
    column.type.precision = m_precision->value();
    column.type.scale = m_scale->value();

    column.type.enumValues.clear();
    if (column.type.kind == K::Enum) {
        const QStringList parts = m_enumValues->text().split(QLatin1Char(','));
        for (QString part : parts) {
            part = part.trimmed();
            if (!part.isEmpty())
                column.type.enumValues.append(part);
        }
    }

    column.nullable = m_nullable->isChecked();
    column.primaryKey = m_primaryKey->isChecked();
    column.unique = m_unique->isChecked();
    column.autoIncrement = m_autoIncrement->isChecked();
    column.defaultValue = m_defaultValue->text().trimmed();
    column.comment = m_comment->text().trimmed();
    return column;
}

Column ColumnEditorDialog::column() const
{
    return build();
}

} // namespace bb