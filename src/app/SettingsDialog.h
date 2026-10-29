#pragma once

#include <QDialog>

class QSpinBox;
class QLineEdit;
class QLabel;

namespace bb {

class DocumentController;

// Preferences for bedbug: primary-key column name format and canvas (sheet) size.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(DocumentController *controller = nullptr,
                           QWidget *parent = nullptr);

    void accept() override;

private:
    void updatePreview();

    DocumentController *m_controller = nullptr;

    QLineEdit *m_formatEdit = nullptr;
    QLabel *m_previewLabel = nullptr;

    QSpinBox *m_pagesXSpin = nullptr;
    QSpinBox *m_pagesYSpin = nullptr;
};

} // namespace bb