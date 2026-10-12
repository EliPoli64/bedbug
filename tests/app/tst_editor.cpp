#include "DocumentController.h"
#include "EntityEditorPanel.h"

#include <QtTest>

using namespace bb;

namespace {

using namespace bb;

CanonicalType bigintType()
{
    CanonicalType t;
    t.kind = CanonicalType::Kind::BigInt;
    return t;
}

} // namespace

class EntityEditorTest : public QObject
{
    Q_OBJECT

private slots:
    void panelReflectsModelChanges();
};

void EntityEditorTest::panelReflectsModelChanges()
{
    DocumentController controller;
    EntityEditorPanel panel;
    panel.setDocument(&controller);

    const Id entityId = controller.addEntity(QStringLiteral("customer"))->id;
    panel.showEntity(entityId);

    QVERIFY(panel.isEnabled());
    QCOMPARE(panel.columnCount(), 0);

    controller.addColumn(entityId, QStringLiteral("id"), bigintType(), false, true, false, true);
    QCOMPARE(panel.columnCount(), 1);
    const Id idColumnId = panel.columnIdAt(0);
    QVERIFY(!idColumnId.isNull());

    controller.addColumn(entityId, QStringLiteral("email"), bigintType(), false, false, true, false);
    QCOMPARE(panel.columnCount(), 2);

    controller.removeColumn(entityId, idColumnId);
    QCOMPARE(panel.columnCount(), 1);

    // Clearing a selection empties the panel.
    panel.clear();
    QVERIFY(!panel.isEnabled());
    QCOMPARE(panel.columnCount(), 0);
}

QTEST_MAIN(EntityEditorTest)

#include "tst_editor.moc"