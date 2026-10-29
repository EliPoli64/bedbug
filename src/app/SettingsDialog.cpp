#include "SettingsDialog.h"

#include "DocumentController.h"
#include "Theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace bb {

namespace {

const QString kPkFormatKey = QStringLiteral("defaultPkFormat");
const QString kAutoGrowKey = QStringLiteral("autoGrowCanvas");
const QString kAutoPkKey = QStringLiteral("autoAddPrimaryKey");
const QString kRestoreSessionKey = QStringLiteral("restoreSessionOnStartup");

const QString kDefaultPkFormat = QStringLiteral("{table}ID");
const QString kDefaultThemeId = QStringLiteral("SoftBlue");

} // namespace

SettingsDialog::SettingsDialog(DocumentController *controller, QWidget *parent,
                               Tab initialTab)
    : QDialog(parent)
    , m_controller(controller)
{
    setWindowTitle(QStringLiteral("Settings"));
    setMinimumWidth(460);
    resize(540, 360);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(createGeneralTab(), QStringLiteral("General"));
    tabs->addTab(createCanvasTab(), QStringLiteral("Canvas"));
    tabs->addTab(createAppearanceTab(), QStringLiteral("Appearance"));
    tabs->addTab(createDeactivationTab(), QStringLiteral("Deactivation"));
    tabs->setCurrentIndex(initialTab);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);

    updatePreview();
}

QWidget *SettingsDialog::createGeneralTab()
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);

    m_formatEdit = new QLineEdit(page);
    m_formatEdit->setObjectName(QStringLiteral("pkFormatEdit"));
    m_formatEdit->setText(QSettings().value(kPkFormatKey, kDefaultPkFormat).toString());

    m_previewLabel = new QLabel(page);
    m_previewLabel->setObjectName(QStringLiteral("pkPreviewLabel"));

    form->addRow(QStringLiteral("Primary key name format"), m_formatEdit);
    form->addRow(QStringLiteral("Preview (table \"customer\")"), m_previewLabel);

    auto *hint = new QLabel(
        QStringLiteral("Use <b>{table}</b> for the lowercased table name, e.g. "
                       "<code>{table}_id</code>."),
        page);
    hint->setWordWrap(true);
    form->addRow(hint);

    connect(m_formatEdit, &QLineEdit::textChanged, this, &SettingsDialog::updatePreview);
    return page;
}

QWidget *SettingsDialog::createCanvasTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *layoutGroup = new QGroupBox(QStringLiteral("Page layout"), page);
    auto *form = new QFormLayout(layoutGroup);

    Diagram *diagram = m_controller ? m_controller->activeDiagram() : nullptr;
    const int pagesX = diagram ? diagram->pagesX : 1;
    const int pagesY = diagram ? diagram->pagesY : 1;

    m_pagesXSpin = new QSpinBox(layoutGroup);
    m_pagesXSpin->setObjectName(QStringLiteral("pagesXSpin"));
    m_pagesXSpin->setRange(1, 20);
    m_pagesXSpin->setValue(pagesX);
    m_pagesYSpin = new QSpinBox(layoutGroup);
    m_pagesYSpin->setObjectName(QStringLiteral("pagesYSpin"));
    m_pagesYSpin->setRange(1, 20);
    m_pagesYSpin->setValue(pagesY);

    form->addRow(QStringLiteral("Pages across (A4 landscape)"), m_pagesXSpin);
    form->addRow(QStringLiteral("Pages down"), m_pagesYSpin);

    m_autoGrowCheck = new QCheckBox(
        QStringLiteral("Grow the canvas automatically when a table is dragged past the edge"),
        layoutGroup);
    m_autoGrowCheck->setObjectName(QStringLiteral("autoGrowCheck"));
    m_autoGrowCheck->setChecked(QSettings().value(kAutoGrowKey, true).toBool());
    form->addRow(m_autoGrowCheck);

    layout->addWidget(layoutGroup);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createAppearanceTab()
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);

    m_themeCombo = new QComboBox(page);
    m_themeCombo->setObjectName(QStringLiteral("themeCombo"));
    const QString active = currentThemeId();
    for (const QString &id : availableThemeIds()) {
        m_themeCombo->addItem(themeDisplayName(id), id);
        if (id == active)
            m_themeCombo->setCurrentIndex(m_themeCombo->count() - 1);
    }

    form->addRow(QStringLiteral("Theme"), m_themeCombo);

    auto *hint = new QLabel(
        QStringLiteral("The default <b>Soft Blue</b> theme is a calm dark palette with a "
                       "muted accent. The <i>Dark&nbsp;Blue/Green/Maroon/Purple</i> "
                       "themes are vendored from the qt-dark-theme project and are more "
                       "saturated. The theme is applied when you press OK."),
        page);
    hint->setWordWrap(true);
    form->addRow(hint);

    return page;
}

QWidget *SettingsDialog::createDeactivationTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(QStringLiteral("Turn features off"), page);
    auto *groupLayout = new QVBoxLayout(group);

    m_disableAutoPkCheck = new QCheckBox(
        QStringLiteral("Don't add a primary key column to new tables"), group);
    m_disableAutoPkCheck->setObjectName(QStringLiteral("disableAutoPkCheck"));
    m_disableAutoPkCheck->setChecked(!QSettings().value(kAutoPkKey, true).toBool());

    m_disableSessionRestoreCheck =
        new QCheckBox(QStringLiteral("Don't restore the previous board on startup"), group);
    m_disableSessionRestoreCheck->setObjectName(QStringLiteral("disableSessionRestoreCheck"));
    m_disableSessionRestoreCheck->setChecked(
        !QSettings().value(kRestoreSessionKey, true).toBool());

    groupLayout->addWidget(m_disableAutoPkCheck);
    groupLayout->addWidget(m_disableSessionRestoreCheck);

    layout->addWidget(group);

    auto *resetButton = new QPushButton(QStringLiteral("Restore defaults"), page);
    resetButton->setObjectName(QStringLiteral("restoreDefaultsButton"));
    connect(resetButton, &QPushButton::clicked, this, &SettingsDialog::restoreDefaults);
    layout->addWidget(resetButton);
    layout->addStretch();

    return page;
}

void SettingsDialog::updatePreview()
{
    QString format = m_formatEdit->text().trimmed();
    if (format.isEmpty())
        format = kDefaultPkFormat;
    m_previewLabel->setText(
        format.replace(QStringLiteral("{table}"), QStringLiteral("customer")));
}

void SettingsDialog::restoreDefaults()
{
    m_formatEdit->setText(kDefaultPkFormat);
    m_autoGrowCheck->setChecked(true);
    m_disableAutoPkCheck->setChecked(false);
    m_disableSessionRestoreCheck->setChecked(false);
    if (m_themeCombo->currentData().toString() != kDefaultThemeId) {
        const int index = m_themeCombo->findData(kDefaultThemeId);
        if (index >= 0)
            m_themeCombo->setCurrentIndex(index);
    }
}

void SettingsDialog::accept()
{
    QSettings settings;

    QString format = m_formatEdit->text().trimmed();
    if (format.isEmpty())
        format = kDefaultPkFormat;
    settings.setValue(kPkFormatKey, format);
    settings.setValue(kAutoGrowKey, m_autoGrowCheck->isChecked());
    settings.setValue(kAutoPkKey, !m_disableAutoPkCheck->isChecked());
    settings.setValue(kRestoreSessionKey, !m_disableSessionRestoreCheck->isChecked());

    const QString themeId = m_themeCombo->currentData().toString();
    settings.setValue(QStringLiteral("appearanceTheme"), themeId);
    if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance()))
        applyTheme(app, themeId);

    if (m_controller)
        m_controller->setCanvasSize(m_pagesXSpin->value(), m_pagesYSpin->value());

    QDialog::accept();
}

} // namespace bb
