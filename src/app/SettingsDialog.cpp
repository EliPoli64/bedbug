#include "SettingsDialog.h"

#include "DocumentController.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace bb {

SettingsDialog::SettingsDialog(DocumentController *controller, QWidget *parent)
    : QDialog(parent)
    , m_controller(controller)
{
    setWindowTitle(QStringLiteral("Settings"));

    m_formatEdit = new QLineEdit(this);
    m_formatEdit->setText(QSettings().value(QStringLiteral("defaultPkFormat"),
                                             QStringLiteral("{table}ID")).toString());

    m_previewLabel = new QLabel(this);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Primary key name format"), m_formatEdit);
    form->addRow(QStringLiteral("Preview (table \"customer\")"), m_previewLabel);

    if (m_controller) {
        if (Diagram *diagram = m_controller->activeDiagram()) {
            m_pagesXSpin = new QSpinBox(this);
            m_pagesXSpin->setObjectName(QStringLiteral("pagesXSpin"));
            m_pagesXSpin->setRange(1, 20);
            m_pagesXSpin->setValue(diagram->pagesX);
            m_pagesYSpin = new QSpinBox(this);
            m_pagesYSpin->setObjectName(QStringLiteral("pagesYSpin"));
            m_pagesYSpin->setRange(1, 20);
            m_pagesYSpin->setValue(diagram->pagesY);
            form->addRow(QStringLiteral("Canvas pages (width)"), m_pagesXSpin);
            form->addRow(QStringLiteral("Canvas pages (height)"), m_pagesYSpin);
        }
    }

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

    if (m_controller && m_pagesXSpin && m_pagesYSpin)
        m_controller->setCanvasSize(m_pagesXSpin->value(), m_pagesYSpin->value());

    QDialog::accept();
}

} // namespace bb