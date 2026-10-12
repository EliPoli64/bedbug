#include "SqlGenerator.h"
#include "dialects/Postgres.h"

#include "JsonSerializer.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QtTest>

namespace {

using namespace bb;

const PostgresDialect kPostgres;

QString goldenDir()
{
    return QFileInfo(QString::fromLatin1(__FILE__)).dir()
        .filePath(QStringLiteral("../golden/postgres"));
}

QString expectedSqlPath(const QString &caseName)
{
    return goldenDir() + QLatin1Char('/') + caseName + QStringLiteral(".sql");
}

bool loadModel(const QString &caseName, Model *model, QString *error)
{
    const QString path = goldenDir() + QLatin1Char('/') + caseName + QStringLiteral(".model.json");
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

void verifyCase(const QString &caseName)
{
    Model model;
    QString error;
    QVERIFY2(loadModel(caseName, &model, &error), qPrintable(error));

    const QString generated = SqlGenerator().generate(model, kPostgres, SqlGenOptions{});

    if (qEnvironmentVariableIsSet("BEDBUG_REGENERATE")) {
        QFile out(expectedSqlPath(caseName));
        QVERIFY2(out.open(QIODevice::WriteOnly), qPrintable(out.errorString()));
        out.write(generated.toUtf8());
        out.close();
        QSKIP("BEDBUG_REGENERATE set — fixture rewritten");
    }

    QFile expectedFile(expectedSqlPath(caseName));
    QVERIFY2(expectedFile.open(QIODevice::ReadOnly), qPrintable(expectedFile.errorString()));
    const QString expected = QString::fromUtf8(expectedFile.readAll());

    if (generated != expected) {
        qDebug().noquote().nospace() << "=== GENERATED (" << caseName << ") ===\n" << generated;
        qDebug().noquote().nospace() << "=== EXPECTED (" << caseName << ") ===\n" << expected;
    }
    QCOMPARE(generated, expected);
}

} // namespace

class GoldenTest : public QObject
{
    Q_OBJECT

private slots:
    void simple();
    void compositeFk();
    void cyclic();
    void types();
};

void GoldenTest::simple()
{
    verifyCase(QStringLiteral("simple"));
}

void GoldenTest::compositeFk()
{
    verifyCase(QStringLiteral("composite_fk"));
}

void GoldenTest::cyclic()
{
    verifyCase(QStringLiteral("cyclic"));
}

void GoldenTest::types()
{
    verifyCase(QStringLiteral("types"));
}

QTEST_APPLESS_MAIN(GoldenTest)

#include "tst_golden.moc"