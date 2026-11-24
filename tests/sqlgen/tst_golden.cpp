#include "DialectRegistry.h"
#include "SqlGenerator.h"

#include "JsonSerializer.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QtTest>

namespace {

using namespace bb;

QString goldenRoot()
{
    return QFileInfo(QString::fromLatin1(__FILE__)).dir()
        .filePath(QStringLiteral("../golden"));
}

QString modelPath(const QString &caseName)
{
    return goldenRoot() + QStringLiteral("/models/") + caseName
           + QStringLiteral(".model.json");
}

QString expectedSqlPath(const QString &dialectId, const QString &caseName)
{
    return goldenRoot() + QLatin1Char('/') + dialectId + QLatin1Char('/') + caseName
           + QStringLiteral(".sql");
}

bool loadModel(const QString &caseName, Model *model, QString *error)
{
    const QString path = modelPath(caseName);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("cannot open %1: %2").arg(path, file.errorString());
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        *error = QStringLiteral("invalid JSON in %1").arg(path);
        return false;
    }
    *model = JsonSerializer::fromJson(doc.object(), error);
    return error->isEmpty();
}

void verifyCase(const SqlDialect &dialect, const QString &caseName)
{
    Model model;
    QString error;
    QVERIFY2(loadModel(caseName, &model, &error), qPrintable(error));

    const QString generated = SqlGenerator().generate(model, dialect, SqlGenOptions{});
    const QString path = expectedSqlPath(dialect.id(), caseName);

    if (qEnvironmentVariableIsSet("BEDBUG_REGENERATE")) {
        QFile out(path);
        QVERIFY2(out.open(QIODevice::WriteOnly), qPrintable(out.errorString()));
        out.write(generated.toUtf8());
        out.close();
        QSKIP("BEDBUG_REGENERATE set — fixture rewritten");
    }

    QFile expectedFile(path);
    QVERIFY2(expectedFile.open(QIODevice::ReadOnly), qPrintable(expectedFile.errorString()));
    const QString expected = QString::fromUtf8(expectedFile.readAll());

    if (generated != expected) {
        qDebug().noquote().nospace() << "=== GENERATED (" << dialect.id() << '/' << caseName
                                     << ") ===\n"
                                     << generated;
        qDebug().noquote().nospace() << "=== EXPECTED (" << dialect.id() << '/' << caseName
                                     << ") ===\n"
                                     << expected;
    }
    QCOMPARE(generated, expected);
}

QList<const SqlDialect *> goldenDialects()
{
    return DialectRegistry::all();
}

} // namespace

class GoldenTest : public QObject
{
    Q_OBJECT

private slots:
    void registry();
    void simple();
    void compositeFk();
    void cyclic();
    void types();
};

void GoldenTest::registry()
{
    const QList<const SqlDialect *> dialects = DialectRegistry::all();
    QCOMPARE(dialects.size(), 3);
    QCOMPARE(dialects.first()->id(), QStringLiteral("postgres"));
    QCOMPARE(dialects.at(1)->id(), QStringLiteral("mysql"));
    QCOMPARE(dialects.at(2)->id(), QStringLiteral("sqlserver"));
    QCOMPARE(DialectRegistry::fallback()->id(), QStringLiteral("postgres"));
    QCOMPARE(DialectRegistry::find(QStringLiteral("mysql"))->id(), QStringLiteral("mysql"));
    QCOMPARE(DialectRegistry::find(QStringLiteral("sqlserver"))->id(),
             QStringLiteral("sqlserver"));
    QVERIFY(!DialectRegistry::find(QStringLiteral("nope")));
}

void GoldenTest::simple()
{
    for (const SqlDialect *dialect : goldenDialects())
        verifyCase(*dialect, QStringLiteral("simple"));
}

void GoldenTest::compositeFk()
{
    for (const SqlDialect *dialect : goldenDialects())
        verifyCase(*dialect, QStringLiteral("composite_fk"));
}

void GoldenTest::cyclic()
{
    for (const SqlDialect *dialect : goldenDialects())
        verifyCase(*dialect, QStringLiteral("cyclic"));
}

void GoldenTest::types()
{
    for (const SqlDialect *dialect : goldenDialects())
        verifyCase(*dialect, QStringLiteral("types"));
}

QTEST_APPLESS_MAIN(GoldenTest)

#include "tst_golden.moc"
