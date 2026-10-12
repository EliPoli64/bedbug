#include "DocumentController.h"

#include <QtTest>

namespace {

using namespace bb;

Column makeColumn(const char *idText, const QString &name)
{
    bool ok = false;
    Column column;
    column.id = Id::fromString(QLatin1String(idText), &ok);
    Q_ASSERT(ok);
    column.name = name;
    column.type.kind = CanonicalType::Kind::BigInt;
    column.nullable = false;
    return column;
}

// addEntity() returns a pointer into a QList that reallocates on further appends, so
// tests always re-fetch entities by id from the live model.
Entity *byId(Model *model, const Id &id)
{
    return model->findEntity(id);
}

} // namespace

class DocumentControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void addRelationship_linksTables();
    void addRelationship_rejectsInvalidInput();
    void removeEntity_cascadesRelationshipsAndNodes();
    void removeColumn_removesRelationshipsAndIndexes();
    void columnCrud_addUpdateRemove();
};

void DocumentControllerTest::addRelationship_linksTables()
{
    DocumentController controller;
    const Id childId = controller.addEntity(QStringLiteral("orders"))->id;
    const Id parentId = controller.addEntity(QStringLiteral("customer"))->id;

    Entity *liveChild = byId(controller.model(), childId);
    Entity *liveParent = byId(controller.model(), parentId);
    Column childFk = makeColumn("0a000000-0000-0000-0000-000000000001", "customer_id");
    Column parentPk = makeColumn("0a000000-0000-0000-0000-000000000002", "id");
    liveChild->columns.append(childFk);
    liveParent->columns.append(parentPk);

    Relationship *relationship =
        controller.addRelationship(childId, {childFk.id}, parentId, {parentPk.id});
    QVERIFY(relationship);
    QCOMPARE(relationship->name, QStringLiteral("fk_orders_customer"));
    QCOMPARE(relationship->childEntity, childId);
    QCOMPARE(relationship->parentEntity, parentId);
    QCOMPARE(controller.model()->relationships.size(), 1);

    // Custom name is honored.
    Relationship *named = controller.addRelationship(childId, {childFk.id}, parentId, {parentPk.id},
                                                     QStringLiteral("orders_customer"));
    QVERIFY(named);
    QCOMPARE(controller.model()->relationships.size(), 2);
    QCOMPARE(named->name, QStringLiteral("orders_customer"));
}

void DocumentControllerTest::addRelationship_rejectsInvalidInput()
{
    DocumentController controller;
    const Id childId = controller.addEntity(QStringLiteral("a"))->id;
    const Id parentId = controller.addEntity(QStringLiteral("b"))->id;

    Entity *child = byId(controller.model(), childId);
    Entity *parent = byId(controller.model(), parentId);
    Column childCol = makeColumn("0b000000-0000-0000-0000-000000000001", "x");
    Column parentCol = makeColumn("0b000000-0000-0000-0000-000000000002", "id");
    child->columns.append(childCol);
    parent->columns.append(parentCol);

    // Same entity on both sides.
    QVERIFY(!controller.addRelationship(childId, {childCol.id}, childId, {parentCol.id}));

    // Unknown columns.
    QVERIFY(!controller.addRelationship(childId, {Id::create()}, parentId, {parentCol.id}));
    QVERIFY(!controller.addRelationship(childId, {childCol.id}, parentId, {Id::create()}));

    // Missing entity.
    QVERIFY(!controller.addRelationship(Id::create(), {childCol.id}, parentId, {parentCol.id}));

    // Empty column lists.
    QVERIFY(!controller.addRelationship(childId, {}, parentId, {parentCol.id}));

    QCOMPARE(controller.model()->relationships.size(), 0);
}

void DocumentControllerTest::removeEntity_cascadesRelationshipsAndNodes()
{
    DocumentController controller;
    const Id childId = controller.addEntity(QStringLiteral("orders"))->id;
    const Id parentId = controller.addEntity(QStringLiteral("customer"))->id;

    Entity *child = byId(controller.model(), childId);
    Entity *parent = byId(controller.model(), parentId);
    Column childFk = makeColumn("0c000000-0000-0000-0000-000000000001", "customer_id");
    Column parentPk = makeColumn("0c000000-0000-0000-0000-000000000002", "id");
    child->columns.append(childFk);
    parent->columns.append(parentPk);

    QVERIFY(controller.addRelationship(childId, {childFk.id}, parentId, {parentPk.id}));
    QCOMPARE(controller.model()->relationships.size(), 1);
    QCOMPARE(controller.activeDiagram()->nodes.size(), 2);

    QVERIFY(controller.removeEntity(childId));
    QCOMPARE(controller.model()->entities.size(), 1);
    QCOMPARE(controller.model()->relationships.size(), 0);
    QCOMPARE(controller.activeDiagram()->nodes.size(), 1);

    // Removing a missing entity fails quietly.
    QVERIFY(!controller.removeEntity(Id::create()));
}

void DocumentControllerTest::removeColumn_removesRelationshipsAndIndexes()
{
    DocumentController controller;
    const Id parentId = controller.addEntity(QStringLiteral("customer"))->id;
    const Id childId = controller.addEntity(QStringLiteral("orders"))->id;

    Entity *parent = byId(controller.model(), parentId);
    Entity *child = byId(controller.model(), childId);

    Column parentPk = makeColumn("0d000000-0000-0000-0000-000000000001", "id");
    Column childFk = makeColumn("0d000000-0000-0000-0000-000000000002", "customer_id");
    Column childOther = makeColumn("0d000000-0000-0000-0000-000000000003", "number");
    parent->columns.append(parentPk);
    child->columns.append(childFk);
    child->columns.append(childOther);

    Index index;
    bool idOk = false;
    index.id = Id::fromString(QStringLiteral("0d000000-0000-0000-0000-000000000010"), &idOk);
    Q_ASSERT(idOk);
    index.name = QStringLiteral("ix_orders_number");
    index.columnIds = {childOther.id};
    child->indexes.append(index);

    QVERIFY(controller.addRelationship(childId, {childFk.id}, parentId, {parentPk.id}));
    QCOMPARE(controller.model()->relationships.size(), 1);

    // Removing the FK column drops the relationship.
    QVERIFY(controller.removeColumn(childId, childFk.id));
    QCOMPARE(controller.model()->relationships.size(), 0);

    // Removing an indexed column drops the index.
    QVERIFY(controller.removeColumn(childId, childOther.id));
    QCOMPARE(child->indexes.size(), 0);
    QCOMPARE(child->columns.size(), 0);

    // Unknown column.
    QVERIFY(!controller.removeColumn(childId, Id::create()));
}

void DocumentControllerTest::columnCrud_addUpdateRemove()
{
    DocumentController controller;
    const Id entityId = controller.addEntity(QStringLiteral("customer"))->id;

    CanonicalType bigint;
    bigint.kind = CanonicalType::Kind::BigInt;
    CanonicalType varchar;
    varchar.kind = CanonicalType::Kind::Varchar;
    varchar.length = 255;

    Column *id = controller.addColumn(entityId, QStringLiteral("id"), bigint, false, true, false,
                                      true);
    QVERIFY(id);
    QCOMPARE(controller.model()->findEntity(entityId)->columns.size(), 1);

    Column *email =
        controller.addColumn(entityId, QStringLiteral("email"), varchar, false, false, true, false);
    QVERIFY(email);
    QCOMPARE(controller.model()->findEntity(entityId)->columns.size(), 2);

    // Update: change type + nullability; id must be preserved.
    const Id emailId = email->id;
    Column updated = *email;
    updated.name = QStringLiteral("public_email");
    updated.nullable = true;
    QVERIFY(controller.updateColumn(entityId, updated));
    const Column *reloaded = controller.model()->findEntity(entityId)->findColumn(emailId);
    QVERIFY(reloaded);
    QCOMPARE(reloaded->name, QStringLiteral("public_email"));
    QVERIFY(reloaded->nullable);
    QCOMPARE(reloaded->id, emailId);

    // Rename the entity.
    QVERIFY(controller.renameEntity(entityId, QStringLiteral("clients")));
    QCOMPARE(controller.model()->findEntity(entityId)->name, QStringLiteral("clients"));
    QVERIFY(!controller.renameEntity(entityId, QStringLiteral("   ")));

    // Remove works and cascades the entity's columns.
    QVERIFY(controller.removeColumn(entityId, emailId));
    QCOMPARE(controller.model()->findEntity(entityId)->columns.size(), 1);

    // Unknown entity.
    QVERIFY(!controller.addColumn(Id::create(), QStringLiteral("x"), bigint, true, false, false, false));
}

QTEST_APPLESS_MAIN(DocumentControllerTest)

#include "tst_document.moc"