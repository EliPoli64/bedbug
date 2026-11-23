#pragma once

#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTimer;

namespace bb {

class DocumentController;
class SqlDialect;
class SqlHighlighter;

// Docked, read-only preview of the SQL generated from the current model (PLAN.md §7.1).
// Regeneration is debounced and never mutates the model.
class SqlPreviewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SqlPreviewPanel(QWidget *parent = nullptr);

    // Connects modelChanged -> debounced refresh and refreshes once immediately.
    void setDocument(DocumentController *controller);

    // The dialect must outlive the panel. Defaults to no dialect (empty preview).
    void setDialect(const SqlDialect &dialect);

    // Re-generates immediately, preserving the scroll position.
    void refresh();

    QString sql() const { return m_sql; }

    // Testable save helper; the Save button wraps it with a file dialog.
    bool saveToFile(const QString &path, QString *errorMessage = nullptr) const;

protected:
    void changeEvent(QEvent *event) override;

private:
    void scheduleRefresh();
    void copyToClipboard();
    void chooseFileAndSave();

    DocumentController *m_controller = nullptr;
    const SqlDialect *m_dialect = nullptr;
    QLabel *m_dialectLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QPlainTextEdit *m_editor = nullptr;
    QPushButton *m_copyButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    QTimer *m_refreshTimer = nullptr;
    SqlHighlighter *m_highlighter = nullptr;
    QString m_sql;
};

} // namespace bb
