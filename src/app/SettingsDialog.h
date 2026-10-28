#pragma once

#include <QDialog>

class QLineEdit;
class QLabel;

namespace bb {

// Preferences for bedbug. Currently exposes the primary-key column name format
// (default "{table}ID", {table} replaced with the lowercased table name).
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

protected:
    void accept() override;

private:
    void updatePreview();

    QLineEdit *m_formatEdit = nullptr;
    QLabel *m_previewLabel = nullptr;
};

} // namespace bb