#include "DialectRegistry.h"
#include "DialectSelector.h"
#include "DocumentController.h"
#include "JsonSerializer.h"
#include "SqlDialect.h"
#include "SqlPreviewPanel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace bb;

class DialectSwitchTest : public QObject
{
    Q_OBJECT

private slots:
    void selector_listsRegisteredDialects();
    void selector_emitsOnChange();
    void switchingDialect_updatesPreviewWithoutTouchingModel();
};

void DialectSwitchTest::selector_listsRegisteredDialects()
{
    DialectSelector selector;
    QCOMPARE(selector.count(), 2);
    QCOMPARE(selector.itemData(0).toString(), QStringLiteral("postgres"));
    QCOMPARE(selector.itemData(1).toString(), QStringLiteral("mysql"));
    QCOMPARE(selector.currentDialectId(), QStringLiteral("postgres"));
}

void DialectSwitchTest::selector_emitsOnChange()
{
    DialectSelector selector;
    QSignalSpy spy(&selector, &DialectSelector::dialectSelected);

    selector.setCurrentDialectId(QStringLiteral("mysql"));
    QCOMPARE(selector.currentDialectId(), QStringLiteral("mysql"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toString(), QStringLiteral("mysql"));

    // Re-selecting the current engine does not emit.
    selector.setCurrentDialectId(QStringLiteral("mysql"));
    QCOMPARE(spy.count(), 1);

    // Unknown ids are ignored.
    selector.setCurrentDialectId(QStringLiteral("nope"));
    QCOMPARE(selector.currentDialectId(), QStringLiteral("mysql"));
}

void DialectSwitchTest::switchingDialect_updatesPreviewWithoutTouchingModel()
{
    DocumentController document;
    QVERIFY(document.addEntity(QStringLiteral("customer")));

    SqlPreviewPanel panel;
    panel.setDocument(&document);
    panel.setDialect(*DialectRegistry::find(QStringLiteral("postgres")));

    QVERIFY(panel.sql().contains(QStringLiteral("\"customer\"")));
    QVERIFY(!panel.sql().contains(QStringLiteral("`customer`")));
    const QByteArray before = JsonSerializer::toBytes(*document.model());

    panel.setDialect(*DialectRegistry::find(QStringLiteral("mysql")));

    QVERIFY(panel.sql().contains(QStringLiteral("`customer`")));
    QVERIFY(!panel.sql().contains(QStringLiteral("\"customer\"")));
    QCOMPARE(JsonSerializer::toBytes(*document.model()), before);
}

QTEST_MAIN(DialectSwitchTest)

#include "tst_dialect_switch.moc"
