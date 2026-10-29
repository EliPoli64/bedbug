#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace bb {

class DocumentController;

// Tabbed preferences for bedbug: general behaviour, canvas layout, appearance and
// feature deactivation.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    enum Tab { GeneralTab, CanvasTab, AppearanceTab, DeactivationTab };

    explicit SettingsDialog(DocumentController *controller = nullptr,
                           QWidget *parent = nullptr,
                           Tab initialTab = GeneralTab);

    void accept() override;

private:
    QWidget *createGeneralTab();
    QWidget *createCanvasTab();
    QWidget *createAppearanceTab();
    QWidget *createDeactivationTab();
    void updatePreview();
    void restoreDefaults();

    DocumentController *m_controller = nullptr;

    // General.
    QLineEdit *m_formatEdit = nullptr;
    QLabel *m_previewLabel = nullptr;

    // Canvas.
    QSpinBox *m_pagesXSpin = nullptr;
    QSpinBox *m_pagesYSpin = nullptr;
    QCheckBox *m_autoGrowCheck = nullptr;

    // Appearance.
    QComboBox *m_themeCombo = nullptr;

    // Deactivation.
    QCheckBox *m_disableAutoPkCheck = nullptr;
    QCheckBox *m_disableSessionRestoreCheck = nullptr;
};

} // namespace bb
