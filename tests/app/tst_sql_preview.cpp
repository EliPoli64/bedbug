#include "DocumentController.h"
#include "JsonSerializer.h"
#include "SqlPreviewPanel.h"
#include "dialects/Postgres.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
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

QTEST_MAIN(SqlPreviewTest)

#include "tst_sql_preview.moc"
