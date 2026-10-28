#include "SettingsDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QVBoxLayout>

namespace bb {

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Settings"));

    m_formatEdit = new QLineEdit(this);
    m_formatEdit->setText(QSettings().value(QStringLiteral("defaultPkFormat"),
                                             QStringLiteral("{table}ID")).toString());

    m_previewLabel = new QLabel(this);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Primary key name format"), m_formatEdit);
    form->addRow(QStringLiteral("Preview (table \"customer\")"), m_previewLabel);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(m_formatEdit, &QLineEdit::textChanged, this, &SettingsDialog::updatePreview);
    updatePreview();
}

void SettingsDialog::updatePreview()
{
    QString format = m_formatEdit->text().trimmed();
    if (format.isEmpty())
        format = QStringLiteral("{table}ID");
    m_previewLabel->setText(format.replace(QStringLiteral("{table}"), QStringLiteral("customer")));
}

void SettingsDialog::accept()
{
    const QString format = m_formatEdit->text().trimmed();
    QSettings().setValue(QStringLiteral("defaultPkFormat"),
                        format.isEmpty() ? QStringLiteral("{table}ID") : format);
    QDialog::accept();
}

} // namespace bb