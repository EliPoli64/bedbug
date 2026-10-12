#include "Version.h"

#include <QtTest>

class SmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void qtLinks();
    void versionString_matchesProjectVersion();
};

void SmokeTest::qtLinks()
{
    QVERIFY2(qVersion(), "Qt runtime should be linked");
}

void SmokeTest::versionString_matchesProjectVersion()
{
    QCOMPARE(bb::versionString(), QStringLiteral("0.0.1"));
}

QTEST_APPLESS_MAIN(SmokeTest)

#include "tst_smoke.moc"