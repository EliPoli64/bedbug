#include "DocumentController.h"
#include "JsonSerializer.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

namespace {

using namespace bb;

CanonicalType bigintType()
{
    CanonicalType type;
    type.kind = CanonicalType::Kind::BigInt;
    return type;
}

} // namespace

class UndoTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void addTable_isOneUndoStep();
    void undoAllRedoAll_roundTripsModel();
    void moveTable_mergesConsecutiveMoves();
    void modifiedFlagFollowsCleanState();
    void saveMakesStackClean();
    void newDocument_clearsHistory();
};

void UndoTest::initTestCase()
{
    QSettings().setValue(QStringLiteral("autoAddPrimaryKey"), true);
}

void UndoTest::cleanupTestCase()
{
    QSettings().remove(QStringLiteral("autoAddPrimaryKey"));
}

void UndoTest::addTable_isOneUndoStep()
{
    DocumentController controller;
    QUndoStack *stack = controller.undoStack();
    QVERIFY(stack->isClean());
    QVERIFY(!controller.isModified());

    Entity *entity = controller.addEntity(QStringLiteral("customers"));
    QVERIFY(entity);
    const Id entityId = entity->id;

    // Adding a table (including its default placement) is a single undo step.
    QCOMPARE(stack->count(), 1);
    QCOMPARE(stack->undoText(), QStringLiteral("Add table"));
    QVERIFY(controller.isModified());

    stack->undo();
    QCOMPARE(controller.model()->entities.size(), 0);
    QVERIFY(stack->isClean());
    QVERIFY(!controller.isModified());

    stack->redo();
    QCOMPARE(controller.model()->entities.size(), 1);
    QCOMPARE(controller.model()->entities.first().id, entityId);
    QVERIFY(controller.isModified());
}

void UndoTest::undoAllRedoAll_roundTripsModel()
{
    DocumentController controller;

    const Id customers = controller.addEntity(QStringLiteral("customers"))->id;
    const Id orders = controller.addEntity(QStringLiteral("orders"))->id;

    const Entity *customerEntity = controller.model()->findEntity(customers);
    QVERIFY(customerEntity);
    QVERIFY(!customerEntity->columns.isEmpty());
    const Id customerPk = customerEntity->columns.first().id;

    Column *fk = controller.addColumn(orders, QStringLiteral("customer_id"), bigintType(), true,
                                      false, false, false);
    QVERIFY(fk);
    const Id fkId = fk->id;

    QVERIFY(controller.addRelationship(orders, {fkId}, customers, {customerPk}));
    QVERIFY(controller.renameEntity(orders, QStringLiteral("purchases")));
    controller.setCanvasSize(2, 3);
    controller.addSheetRight();

    QUndoStack *stack = controller.undoStack();
    QCOMPARE(stack->count(), 7);

    const QByteArray expected = JsonSerializer::toBytes(*controller.model());

    while (stack->canUndo())
        stack->undo();

    QCOMPARE(controller.model()->entities.size(), 0);
    QCOMPARE(controller.model()->relationships.size(), 0);
    QCOMPARE(controller.activeDiagram()->pagesX, 1);
    QCOMPARE(controller.activeDiagram()->pagesY, 1);
    QVERIFY(stack->isClean());
    QVERIFY(!controller.isModified());

    while (stack->canRedo())
        stack->redo();

    QCOMPARE(JsonSerializer::toBytes(*controller.model()), expected);
    QVERIFY(!stack->isClean());
    QVERIFY(controller.isModified());
}

void UndoTest::moveTable_mergesConsecutiveMoves()
{
    DocumentController controller;
    Entity *entity = controller.addEntity(QStringLiteral("customers"));
    QVERIFY(entity);
    const Id entityId = entity->id;

    QUndoStack *stack = controller.undoStack();
    QCOMPARE(stack->count(), 1);

    const QPointF start = controller.activeDiagram()->nodes.first().pos;
    controller.moveEntityNode(entityId, start + QPointF(10, 0));
    controller.moveEntityNode(entityId, start + QPointF(20, 0));
    controller.moveEntityNode(entityId, start + QPointF(30, 40));

    // Every pixel of one drag collapses into a single undo step.
    QCOMPARE(stack->count(), 2);
    QCOMPARE(stack->undoText(), QStringLiteral("Move table"));
    QCOMPARE(controller.activeDiagram()->nodes.first().pos, start + QPointF(30, 40));

    stack->undo();
    QCOMPARE(controller.activeDiagram()->nodes.first().pos, start);
    QCOMPARE(controller.model()->entities.size(), 1);

    stack->undo();
    QCOMPARE(controller.model()->entities.size(), 0);

    stack->redo();
    QCOMPARE(controller.model()->entities.size(), 1);
    stack->redo();
    QCOMPARE(controller.activeDiagram()->nodes.first().pos, start + QPointF(30, 40));
}

void UndoTest::modifiedFlagFollowsCleanState()
{
    DocumentController controller;
    QSignalSpy spy(&controller, &DocumentController::modifiedChanged);

    QVERIFY(!controller.isModified());
    controller.addEntity(QStringLiteral("customers"));
    QVERIFY(controller.isModified());
    QCOMPARE(spy.count(), 1);

    controller.undoStack()->undo();
    QVERIFY(!controller.isModified());
    QCOMPARE(spy.count(), 2);

    controller.undoStack()->redo();
    QVERIFY(controller.isModified());
    QCOMPARE(spy.count(), 3);

    // Canvas auto-grow is out-of-band but must keep the document dirty.
    controller.undoStack()->undo();
    QVERIFY(!controller.isModified());
    controller.markDirty();
    QVERIFY(controller.isModified());
}

void UndoTest::saveMakesStackClean()
{
    DocumentController controller;
    controller.addEntity(QStringLiteral("customers"));
    QVERIFY(controller.isModified());

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("model.bedbug.json"));
    QString error;
    QVERIFY2(controller.saveToFile(path, &error), qPrintable(error));

    QVERIFY(!controller.isModified());
    QVERIFY(controller.undoStack()->isClean());

    // Undoing relative to the saved state makes the document dirty again.
    controller.undoStack()->undo();
    QVERIFY(controller.isModified());
    QVERIFY(!controller.undoStack()->isClean());

    controller.undoStack()->redo();
    QVERIFY(!controller.isModified());
    QVERIFY(controller.undoStack()->isClean());
}

void UndoTest::newDocument_clearsHistory()
{
    DocumentController controller;
    controller.addEntity(QStringLiteral("customers"));
    QVERIFY(controller.undoStack()->canUndo());

    controller.newDocument();
    QVERIFY(!controller.undoStack()->canUndo());
    QVERIFY(!controller.undoStack()->canRedo());
    QVERIFY(!controller.isModified());
    QCOMPARE(controller.model()->entities.size(), 0);
    QCOMPARE(controller.model()->diagrams.size(), 1);
}

QTEST_MAIN(UndoTest)

#include "tst_undo.moc"
