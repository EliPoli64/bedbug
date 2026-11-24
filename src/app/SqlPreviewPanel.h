#pragma once

#include "SqlGenerator.h"

#include <QList>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTimer;
class QToolButton;

namespace bb {

class DocumentController;
class SqlDialect;
class SqlHighlighter;

// Docked, read-only preview of the SQL generated from the current model (PLAN.md §7.1).
// Regeneration is debounced and never mutates the model. The fidelity chip summarises
// how many column types the target engine had to adapt (§7.3) and "Resolve all" records
// those choices as per-engine overrides in one undo step (§7.4).
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

    // Columns the current engine cannot represent exactly (empty when everything maps).
    QList<TypeAdvisory> advisories() const { return m_advisories; }
    int advisoryCount() const { return static_cast<int>(m_advisories.size()); }

    // Chip text, e.g. "PostgreSQL ✓" or "SQL Server ⚠ 3 type(s) adapted".
    QString fidelityText() const;

    // Testable action behind the "Resolve all" button: writes every advisory as an
    // override for this dialect in one undo step. Returns the number of columns changed.
    int resolveAll();

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
    QToolButton *m_fidelityChip = nullptr;
    QPlainTextEdit *m_editor = nullptr;
    QPushButton *m_copyButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_resolveAllButton = nullptr;
    QTimer *m_refreshTimer = nullptr;
    SqlHighlighter *m_highlighter = nullptr;
    QList<TypeAdvisory> m_advisories;
    QString m_sql;
};

} // namespace bb
