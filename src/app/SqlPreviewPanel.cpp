#include "SqlPreviewPanel.h"

#include "DocumentController.h"
#include "SqlDialect.h"
#include "SqlGenerator.h"

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QMessageBox>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

namespace bb {

namespace {

QStringList sqlKeywords()
{
    return {
        QStringLiteral("ADD"),        QStringLiteral("ALWAYS"),
        QStringLiteral("ALTER"),      QStringLiteral("AS"),
        QStringLiteral("BEGIN"),      QStringLiteral("BIGINT"),
        QStringLiteral("BIGSERIAL"),  QStringLiteral("BOOLEAN"),
        QStringLiteral("BYTEA"),      QStringLiteral("CASCADE"),
        QStringLiteral("CHAR"),       QStringLiteral("CHARACTER"),
        QStringLiteral("CHECK"),      QStringLiteral("COLUMN"),
        QStringLiteral("COMMENT"),    QStringLiteral("COMMIT"),
        QStringLiteral("CONSTRAINT"), QStringLiteral("CREATE"),
        QStringLiteral("DATE"),       QStringLiteral("DECIMAL"),
        QStringLiteral("DEFAULT"),    QStringLiteral("DELETE"),
        QStringLiteral("DOUBLE"),     QStringLiteral("DROP"),
        QStringLiteral("ENUM"),       QStringLiteral("EXISTS"),
        QStringLiteral("FOREIGN"),    QStringLiteral("GENERATED"),
        QStringLiteral("IDENTITY"),   QStringLiteral("IF"),
        QStringLiteral("INDEX"),      QStringLiteral("INTEGER"),
        QStringLiteral("JSON"),       QStringLiteral("JSONB"),
        QStringLiteral("KEY"),        QStringLiteral("NOT"),
        QStringLiteral("NULL"),       QStringLiteral("NUMERIC"),
        QStringLiteral("ON"),         QStringLiteral("PRECISION"),
        QStringLiteral("PRIMARY"),    QStringLiteral("REAL"),
        QStringLiteral("REFERENCES"), QStringLiteral("RESTRICT"),
        QStringLiteral("SERIAL"),     QStringLiteral("SET"),
        QStringLiteral("SMALLINT"),   QStringLiteral("TABLE"),
        QStringLiteral("TEXT"),       QStringLiteral("TIME"),
        QStringLiteral("TIMESTAMP"),  QStringLiteral("TYPE"),
        QStringLiteral("UNIQUE"),     QStringLiteral("UPDATE"),
        QStringLiteral("USING"),      QStringLiteral("UUID"),
        QStringLiteral("VARYING"),    QStringLiteral("VARCHAR"),
        QStringLiteral("WITH"),       QStringLiteral("WITHOUT"),
        QStringLiteral("ZONE"),
    };
}

} // namespace

// Minimal SQL highlighter: keywords, identifiers, numbers, strings and comments.
// Colours follow the active palette so every bundled theme stays readable.
class SqlHighlighter : public QSyntaxHighlighter
{
public:
    explicit SqlHighlighter(QTextDocument *document)
        : QSyntaxHighlighter(document)
    {
        updateColors();
    }

    void updateColors()
    {
        const QColor base = QApplication::palette().color(QPalette::Base);
        const bool dark = base.lightness() < 128;

        QTextCharFormat keyword;
        keyword.setForeground(dark ? QColor(138, 180, 248) : QColor(59, 91, 155));
        keyword.setFontWeight(QFont::DemiBold);

        QTextCharFormat identifier;
        identifier.setForeground(dark ? QColor(197, 210, 230) : QColor(74, 85, 104));

        QTextCharFormat number;
        number.setForeground(dark ? QColor(240, 176, 114) : QColor(180, 83, 9));

        QTextCharFormat string;
        string.setForeground(dark ? QColor(165, 214, 167) : QColor(46, 125, 50));

        QTextCharFormat comment;
        comment.setForeground(dark ? QColor(154, 160, 166) : QColor(107, 114, 128));
        comment.setFontItalic(true);

        m_rules.clear();
        m_rules.append({QRegularExpression(
                            QStringLiteral("\\b(?:%1)\\b").arg(sqlKeywords().join(QLatin1Char('|'))),
                            QRegularExpression::CaseInsensitiveOption),
                        keyword});
        m_rules.append({QRegularExpression(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b")), number});
        m_rules.append({QRegularExpression(QStringLiteral("\"[^\"]*\"")), identifier});
        m_rules.append({QRegularExpression(QStringLiteral("'(?:[^']|'')*'")), string});
        m_rules.append({QRegularExpression(QStringLiteral("--[^\n]*")), comment});

        rehighlight();
    }

protected:
    void highlightBlock(const QString &text) override
    {
        for (const Rule &rule : m_rules) {
            auto matches = rule.pattern.globalMatch(text);
            while (matches.hasNext()) {
                const QRegularExpressionMatch match = matches.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }
    }

private:
    struct Rule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QList<Rule> m_rules;
};

SqlPreviewPanel::SqlPreviewPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    auto *header = new QHBoxLayout;
    header->setSpacing(8);

    m_dialectLabel = new QLabel(tr("No engine"), this);
    QFont dialectFont = m_dialectLabel->font();
    dialectFont.setBold(true);
    m_dialectLabel->setFont(dialectFont);

    m_fidelityChip = new QToolButton(this);
    m_fidelityChip->setObjectName(QStringLiteral("sqlFidelityChip"));
    m_fidelityChip->setAutoRaise(true);
    m_fidelityChip->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_fidelityChip->setToolTip(tr("Column types adapted for this engine; use Resolve all to record them"));

    m_resolveAllButton = new QPushButton(tr("Resolve all"), this);
    m_resolveAllButton->setObjectName(QStringLiteral("resolveAllButton"));
    m_resolveAllButton->setToolTip(
        tr("Record the adapted types as overrides for this engine (one undo step)"));
    m_resolveAllButton->setEnabled(false);

    m_copyButton = new QPushButton(tr("Copy"), this);
    m_copyButton->setObjectName(QStringLiteral("sqlCopyButton"));
    m_copyButton->setEnabled(false);
    m_saveButton = new QPushButton(tr("Save…"), this);
    m_saveButton->setObjectName(QStringLiteral("sqlSaveButton"));
    m_saveButton->setEnabled(false);

    header->addWidget(m_dialectLabel);
    header->addWidget(m_fidelityChip);
    header->addStretch(1);
    header->addWidget(m_resolveAllButton);
    header->addWidget(m_copyButton);
    header->addWidget(m_saveButton);
    layout->addLayout(header);

    m_editor = new QPlainTextEdit(this);
    m_editor->setObjectName(QStringLiteral("sqlPreviewText"));
    m_editor->setReadOnly(true);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(m_editor, 1);

    m_highlighter = new SqlHighlighter(m_editor->document());

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(150);
    connect(m_refreshTimer, &QTimer::timeout, this, &SqlPreviewPanel::refresh);

    connect(m_copyButton, &QPushButton::clicked, this, &SqlPreviewPanel::copyToClipboard);
    connect(m_saveButton, &QPushButton::clicked, this, &SqlPreviewPanel::chooseFileAndSave);
    connect(m_resolveAllButton, &QPushButton::clicked, this, &SqlPreviewPanel::resolveAll);

    setMinimumHeight(140);
    refresh();
}

void SqlPreviewPanel::setDocument(DocumentController *controller)
{
    if (m_controller)
        disconnect(m_controller, nullptr, this, nullptr);

    m_controller = controller;
    if (m_controller) {
        connect(m_controller, &DocumentController::modelChanged, this,
                &SqlPreviewPanel::scheduleRefresh);
    }
    refresh();
}

void SqlPreviewPanel::setDialect(const SqlDialect &dialect)
{
    m_dialect = &dialect;
    m_dialectLabel->setText(dialect.displayName());
    refresh();
}

QString SqlPreviewPanel::fidelityText() const
{
    if (!m_dialect)
        return {};
    const QString name = m_dialect->displayName();
    if (m_controller && m_controller->model()->entities.isEmpty())
        return tr("No tables");
    if (m_advisories.isEmpty())
        return QStringLiteral("%1 ✓").arg(name);
    return tr("%1 ⚠ %n type(s) adapted", nullptr, advisoryCount()).arg(name);
}

void SqlPreviewPanel::refresh()
{
    m_refreshTimer->stop();

    const int scroll = m_editor->verticalScrollBar()->value();

    if (!m_controller || !m_dialect) {
        m_sql.clear();
        m_editor->clear();
        m_advisories.clear();
        m_fidelityChip->setText(QString());
        m_fidelityChip->setToolTip(QString());
        m_resolveAllButton->setEnabled(false);
        m_copyButton->setEnabled(false);
        m_saveButton->setEnabled(false);
        return;
    }

    const Model &model = *m_controller->model();
    const SqlGenerator generator;

    if (model.entities.isEmpty()) {
        m_sql.clear();
        m_editor->setPlainText(tr("-- No tables yet. Add one to see SQL here."));
        m_advisories.clear();
        m_fidelityChip->setText(fidelityText());
        m_fidelityChip->setToolTip(QString());
        m_resolveAllButton->setEnabled(false);
        m_copyButton->setEnabled(false);
        m_saveButton->setEnabled(false);
        return;
    }

    m_sql = generator.generate(model, *m_dialect);
    m_advisories = generator.auditTypes(model, *m_dialect);
    m_editor->setPlainText(m_sql);
    m_fidelityChip->setText(fidelityText());

    // Tooltip groups the first few affected columns so the chip is self-explanatory.
    QStringList details;
    const int shown = qMin<int>(6, m_advisories.size());
    for (int i = 0; i < shown; ++i) {
        const TypeAdvisory &advisory = m_advisories.at(i);
        QString line = QStringLiteral("%1.%2: %3 → %4 (%5)")
                           .arg(advisory.entityName, advisory.columnName,
                                advisory.canonicalType, advisory.resolvedType,
                                fidelityName(advisory.fidelity));
        if (!advisory.note.isEmpty())
            line += QStringLiteral(" — ") + advisory.note;
        details.append(line);
    }
    if (m_advisories.size() > shown)
        details.append(tr("… and %n more", nullptr, m_advisories.size() - shown));
    m_fidelityChip->setToolTip(details.join(QLatin1Char('\n')));

    m_resolveAllButton->setEnabled(!m_advisories.isEmpty());
    m_copyButton->setEnabled(true);
    m_saveButton->setEnabled(true);

    m_editor->verticalScrollBar()->setValue(scroll);
}

int SqlPreviewPanel::resolveAll()
{
    if (!m_controller || !m_dialect || m_advisories.isEmpty())
        return 0;

    QList<ColumnTypeOverride> overrides;
    overrides.reserve(m_advisories.size());
    for (const TypeAdvisory &advisory : std::as_const(m_advisories)) {
        overrides.append(
            ColumnTypeOverride{advisory.entityId, advisory.columnId, advisory.resolvedType});
    }

    const int applied = m_controller->applyTypeOverrides(m_dialect->id(), overrides);
    if (applied > 0)
        refresh();
    return applied;
}

void SqlPreviewPanel::scheduleRefresh()
{
    m_refreshTimer->start();
}

void SqlPreviewPanel::copyToClipboard()
{
    if (m_sql.isEmpty())
        return;

    QApplication::clipboard()->setText(m_sql);
    m_copyButton->setText(tr("Copied"));
    QTimer::singleShot(1200, this, [this] { m_copyButton->setText(tr("Copy")); });
}

void SqlPreviewPanel::chooseFileAndSave()
{
    QString suggested = QStringLiteral("schema.sql");
    if (m_controller && m_controller->hasFile()) {
        const QFileInfo document(m_controller->filePath());
        suggested = document.completeBaseName() + QStringLiteral(".sql");
    }

    QString path = QFileDialog::getSaveFileName(
        this, tr("Save SQL"), suggested, tr("SQL files (*.sql);;All files (*)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".sql");

    QString error;
    if (!saveToFile(path, &error))
        QMessageBox::warning(this, tr("Save SQL"),
                             tr("Could not save '%1':\n%2").arg(path, error));
}

bool SqlPreviewPanel::saveToFile(const QString &path, QString *errorMessage) const
{
    if (m_sql.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("There is no SQL to save yet.");
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }

    if (file.write(m_sql.toUtf8()) < 0 || !file.commit()) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    return true;
}

void SqlPreviewPanel::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange
        || event->type() == QEvent::ApplicationPaletteChange) {
        if (m_highlighter)
            m_highlighter->updateColors();
    }
    QWidget::changeEvent(event);
}

} // namespace bb
