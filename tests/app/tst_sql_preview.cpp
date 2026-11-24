#include "DocumentController.h"
#include "JsonSerializer.h"
#include "SqlPreviewPanel.h"
#include "dialects/Postgres.h"
#include "dialects/SqlServer.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QToolButton>
#include <QUndoStack>
#include <QtTest>

using namespace bb;

namespace {

CanonicalType varcharType(int length = 255)
{
    CanonicalType type;
    type.kind = CanonicalType::Kind::Varchar;
    type.length = length;
    return type;
}

CanonicalType canonical(CanonicalType::Kind kind)
{
    CanonicalType type;
    type.kind = kind;
    return type;
}

QToolButton *chipOf(SqlPreviewPanel &panel)
{
    return panel.findChild<QToolButton *>(QStringLiteral("sqlFidelityChip"));
}

QPushButton *resolveAllButtonOf(SqlPreviewPanel &panel)
{
    return panel.findChild<QPushButton *>(QStringLiteral("resolveAllButton"));
}

QPlainTextEdit *editorOf(SqlPreviewPanel &panel)
{
    return panel.findChild<QPlainTextEdit *>(QStringLiteral("sqlPreviewText"));
}

} // namespace

class SqlPreviewTest : public QObject
{
    Q_OBJECT

private slots:
    void emptyModel_showsPlaceholder();
    void tableChange_updatesPreviewLive();
    void generation_doesNotMutateModel();
    void copy_putsSqlOnClipboard();
    void saveToFile_writesGeneratedSql();
    void saveToFile_refusesEmptyPreview();
    void fidelityChip_summarizesAdvisories();
    void fidelityChip_isCleanForPostgres();
    void resolveAll_writesOverridesAndKeepsSql();
    void resolveAll_isOneUndoStep();
    void resolveAll_isNoOpWhenNothingToResolve();
};

void SqlPreviewTest::emptyModel_showsPlaceholder()
{
    DocumentController document;
    PostgresDialect dialect;
    SqlPreviewPanel panel;
    panel.setDialect(dialect);
    panel.setDocument(&document);

    QVERIFY(panel.sql().isEmpty());
    auto *editor = editorOf(panel);
    QVERIFY(editor);
    QVERIFY(editor->toPlainText().contains(QStringLiteral("No tables yet")));

    auto *copy = panel.findChild<QPushButton *>(QStringLiteral("sqlCopyButton"));
    auto *save = panel.findChild<QPushButton *>(QStringLiteral("sqlSaveButton"));
    QVERIFY(copy);
    QVERIFY(save);
    QVERIFY(!copy->isEnabled());
    QVERIFY(!save->isEnabled());
}

void SqlPreviewTest::tableChange_updatesPreviewLive()
{
    DocumentController document;
    PostgresDialect dialect;
    SqlPreviewPanel panel;
    panel.setDialect(dialect);
    panel.setDocument(&document);

    Entity *customer = document.addEntity(QStringLiteral("customer"));
    QVERIFY(customer);
    const Id customerId = customer->id;

    QTRY_VERIFY_WITH_TIMEOUT(panel.sql().contains(QStringLiteral("\"customer\"")), 2000);
    QVERIFY(panel.sql().contains(QStringLiteral("CREATE TABLE")));

    document.addColumn(customerId, QStringLiteral("email"), varcharType(), false, false, true,
                       false);
    QTRY_VERIFY_WITH_TIMEOUT(panel.sql().contains(QStringLiteral("\"email\"")), 2000);

    auto *editor = editorOf(panel);
    QVERIFY(editor);
    QTRY_VERIFY_WITH_TIMEOUT(editor->toPlainText().contains(QStringLiteral("\"email\"")), 2000);
}

void SqlPreviewTest::generation_doesNotMutateModel()
{
    DocumentController document;
    PostgresDialect dialect;
    SqlPreviewPanel panel;
    panel.setDialect(dialect);
    panel.setDocument(&document);

    Entity *entity = document.addEntity(QStringLiteral("orders"));
    QVERIFY(entity);
    document.addColumn(entity->id, QStringLiteral("total"), varcharType(32), false, false,
                       false, false);

    const QByteArray before = JsonSerializer::toBytes(*document.model());
    panel.refresh();
    const QByteArray after = JsonSerializer::toBytes(*document.model());

    QCOMPARE(after, before);
    QVERIFY(panel.sql().contains(QStringLiteral("CREATE TABLE")));
}

void SqlPreviewTest::copy_putsSqlOnClipboard()
{
    DocumentController document;
    PostgresDialect dialect;
    SqlPreviewPanel panel;
    panel.setDialect(dialect);
    panel.setDocument(&document);

    QVERIFY(document.addEntity(QStringLiteral("customer")));
    QTRY_VERIFY_WITH_TIMEOUT(!panel.sql().isEmpty(), 2000);

    auto *copy = panel.findChild<QPushButton *>(QStringLiteral("sqlCopyButton"));
    QVERIFY(copy);
    QVERIFY(copy->isEnabled());
    copy->click();

    QCOMPARE(QApplication::clipboard()->text(), panel.sql());
}

void SqlPreviewTest::saveToFile_writesGeneratedSql()
{
    DocumentController document;
    PostgresDialect dialect;
    SqlPreviewPanel panel;
    panel.setDialect(dialect);
    panel.setDocument(&document);

    QVERIFY(document.addEntity(QStringLiteral("customer")));
    QTRY_VERIFY_WITH_TIMEOUT(!panel.sql().isEmpty(), 2000);

    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = temporary.filePath(QStringLiteral("schema.sql"));

    QString error;
    QVERIFY2(panel.saveToFile(path, &error), qPrintable(error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(file.readAll()), panel.sql());
}

void SqlPreviewTest::saveToFile_refusesEmptyPreview()
{
    SqlPreviewPanel panel;
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());

    QString error;
    QVERIFY(!panel.saveToFile(temporary.filePath(QStringLiteral("empty.sql")), &error));
    QVERIFY(!error.isEmpty());
}

void SqlPreviewTest::fidelityChip_summarizesAdvisories()
{
    DocumentController document;
    SqlServerDialect dialect;
    SqlPreviewPanel panel;
    panel.setDialect(dialect);
    panel.setDocument(&document);

    Entity *entity = document.addEntity(QStringLiteral("event"));
    QVERIFY(entity);
    document.addColumn(entity->id, QStringLiteral("payload"), canonical(CanonicalType::Kind::Json),
                       false, false, false, false);
    panel.refresh();

    QCOMPARE(panel.advisoryCount(), 1);
    auto *chip = chipOf(panel);
    QVERIFY(chip);
    QVERIFY2(chip->text().contains(QStringLiteral("SQL Server")), qPrintable(chip->text()));
    QVERIFY2(chip->text().contains(QStringLiteral("⚠")), qPrintable(chip->text()));
    QVERIFY2(chip->text().contains(QStringLiteral("1")), qPrintable(chip->text()));

    auto *resolve = resolveAllButtonOf(panel);
    QVERIFY(resolve);
    QVERIFY(resolve->isEnabled());
    QVERIFY(chip->toolTip().contains(QStringLiteral("payload")));
}

void SqlPreviewTest::fidelityChip_isCleanForPostgres()
{
    DocumentController document;
    PostgresDialect dialect;
    SqlPreviewPanel panel;
    panel.setDialect(dialect);
    panel.setDocument(&document);

    Entity *entity = document.addEntity(QStringLiteral("event"));
    QVERIFY(entity);
    document.addColumn(entity->id, QStringLiteral("payload"), canonical(CanonicalType::Kind::Json),
                       false, false, false, false);
    panel.refresh();

    QCOMPARE(panel.advisoryCount(), 0);
    auto *chip = chipOf(panel);
    QVERIFY(chip);
    QCOMPARE(chip->text(), QStringLiteral("PostgreSQL ✓"));
    QVERIFY(!resolveAllButtonOf(panel)->isEnabled());
}

void SqlPreviewTest::resolveAll_writesOverridesAndKeepsSql()
{
    DocumentController document;
    SqlServerDialect dialect;
    SqlPreviewPanel panel;
    panel.setDialect(dialect);
    panel.setDocument(&document);

    Entity *entity = document.addEntity(QStringLiteral("event"));
    QVERIFY(entity);
    document.addColumn(entity->id, QStringLiteral("payload"), canonical(CanonicalType::Kind::Json),
                       false, false, false, false);

    panel.refresh();
    QVERIFY(panel.advisoryCount() > 0);
    const QString sqlBefore = panel.sql();
    QVERIFY(!sqlBefore.isEmpty());

    QCOMPARE(panel.resolveAll(), 1);
    QCOMPARE(panel.advisoryCount(), 0);
    QCOMPARE(panel.sql(), sqlBefore);
    QCOMPARE(chipOf(panel)->text(), QStringLiteral("SQL Server ✓"));

    // The accepted decision is stored per engine, so it survives a reload.
    const Model reloaded = JsonSerializer::fromJson(JsonSerializer::toJson(*document.model()));
    const Entity *saved = reloaded.findEntity(entity->id);
    QVERIFY(saved);
    const Column *payload = saved->findColumn(entity->columns.last().id);
    QVERIFY(payload);
    QCOMPARE(payload->dialectOverrides.value(QStringLiteral("sqlserver")),
             QStringLiteral("nvarchar(max)"));
}

void SqlPreviewTest::resolveAll_isOneUndoStep()
{
    DocumentController document;
    SqlServerDialect dialect;
    SqlPreviewPanel panel;
    panel.setDialect(dialect);
    panel.setDocument(&document);

    Entity *entity = document.addEntity(QStringLiteral("event"));
    QVERIFY(entity);
    document.addColumn(entity->id, QStringLiteral("payload"), canonical(CanonicalType::Kind::Json),
                       false, false, false, false);
    document.addColumn(entity->id, QStringLiteral("seen_at"),
                       canonical(CanonicalType::Kind::Timestamp), false, false, false, false);
    panel.refresh();
    QCOMPARE(panel.advisoryCount(), 2);

    const int undoCount = document.undoStack()->count();
    QCOMPARE(panel.resolveAll(), 2);
    QCOMPARE(document.undoStack()->count(), undoCount + 1);
    QCOMPARE(panel.advisoryCount(), 0);

    document.undoStack()->undo();
    panel.refresh();
    QCOMPARE(panel.advisoryCount(), 2);
}

void SqlPreviewTest::resolveAll_isNoOpWhenNothingToResolve()
{
    DocumentController document;
    PostgresDialect dialect;
    SqlPreviewPanel panel;
    panel.setDialect(dialect);
    panel.setDocument(&document);

    Entity *entity = document.addEntity(QStringLiteral("customer"));
    QVERIFY(entity);
    document.addColumn(entity->id, QStringLiteral("email"), varcharType(), false, false, true,
                       false);

    const int undoCount = document.undoStack()->count();
    QCOMPARE(panel.resolveAll(), 0);
    QCOMPARE(document.undoStack()->count(), undoCount);
}

QTEST_MAIN(SqlPreviewTest)

#include "tst_sql_preview.moc"
