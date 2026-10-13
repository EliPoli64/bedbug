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
    void duplicateColumnNamesRejected();
    void createManyToMany_buildsJunction();
    void addRelationshipOneToOne_marksChildUnique();
    void addRelationshipPick_autoCreatesColumn();
    void addRelationshipPick_reusesExistingColumn();
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

void DocumentControllerTest::duplicateColumnNamesRejected()
{
    DocumentController controller;
    const Id entityId = controller.addEntity(QStringLiteral("customer"))->id;

    CanonicalType bigint;
    bigint.kind = CanonicalType::Kind::BigInt;

    QVERIFY(controller.addColumn(entityId, QStringLiteral("id"), bigint, true, false, false, false));
    // Case-insensitive duplicate is rejected.
    QVERIFY(!controller.addColumn(entityId, QStringLiteral("ID"), bigint, true, false, false, false));
    // Empty names are rejected.
    QVERIFY(!controller.addColumn(entityId, QStringLiteral("   "), bigint, true, false, false, false));

    const Id emailId =
        controller.addColumn(entityId, QStringLiteral("email"), bigint, true, false, false, false)->id;

    // Renaming onto a duplicate fails; renaming to a fresh name succeeds.
    Column renamed = *controller.model()->findEntity(entityId)->findColumn(emailId);
    renamed.name = QStringLiteral("Id");
    QVERIFY(!controller.updateColumn(entityId, renamed));

    renamed.name = QStringLiteral("public_email");
    QVERIFY(controller.updateColumn(entityId, renamed));
    QCOMPARE(controller.model()->findEntity(entityId)->columns.size(), 2);
}

void DocumentControllerTest::createManyToMany_buildsJunction()
{
    DocumentController controller;
    const Id leftId = controller.addEntity(QStringLiteral("orders"))->id;
    const Id rightId = controller.addEntity(QStringLiteral("products"))->id;

    Entity *left = byId(controller.model(), leftId);
    Entity *right = byId(controller.model(), rightId);
    Column leftPk = makeColumn("0e000000-0000-0000-0000-000000000001", "id");
    leftPk.primaryKey = true;
    Column rightPk = makeColumn("0e000000-0000-0000-0000-000000000002", "id");
    rightPk.primaryKey = true;
    left->columns.append(leftPk);
    right->columns.append(rightPk);
    QCOMPARE(controller.activeDiagram()->nodes.size(), 2);

    Entity *junction = controller.createManyToMany(leftId, rightId);
    QVERIFY(junction);
    QCOMPARE(junction->name, QStringLiteral("orders_products"));
    QCOMPARE(junction->columns.size(), 2);
    QVERIFY(junction->columns.at(0).primaryKey);
    QVERIFY(junction->columns.at(1).primaryKey);
    QCOMPARE(junction->columns.at(0).name, QStringLiteral("orders_id"));
    QCOMPARE(junction->columns.at(1).name, QStringLiteral("products_id"));

    QCOMPARE(controller.model()->entities.size(), 3);
    QCOMPARE(controller.model()->relationships.size(), 2);
    for (const Relationship &relationship : controller.model()->relationships)
        QVERIFY(relationship.identifying);
    QCOMPARE(controller.activeDiagram()->nodes.size(), 3);

    // Same table for both sides is invalid.
    QVERIFY(!controller.createManyToMany(leftId, leftId));
}

void DocumentControllerTest::addRelationshipOneToOne_marksChildUnique()
{
    DocumentController controller;
    const Id parentId = controller.addEntity(QStringLiteral("customer"))->id;
    const Id childId = controller.addEntity(QStringLiteral("passport"))->id;

    Entity *parent = byId(controller.model(), parentId);
    Entity *child = byId(controller.model(), childId);
    Column parentPk = makeColumn("0f000000-0000-0000-0000-000000000001", "id");
    parentPk.primaryKey = true;
    Column childFk = makeColumn("0f000000-0000-0000-0000-000000000002", "customer_id");
    parent->columns.append(parentPk);
    child->columns.append(childFk);
    QVERIFY(!child->columns.first().unique);

    Relationship *relationship =
        controller.addRelationship(RelationshipKind::OneToOne, childId, {childFk.id}, parentId,
                                   {parentPk.id});
    QVERIFY(relationship);
    QVERIFY(relationship->identifying);
    QVERIFY(child->columns.first().unique);
}

void DocumentControllerTest::addRelationshipPick_autoCreatesColumn()
{
    DocumentController controller;
    const Id parentId = controller.addEntity(QStringLiteral("customer"))->id;
    const Id childId = controller.addEntity(QStringLiteral("orders"))->id;

    Entity *parent = byId(controller.model(), parentId);
    Entity *child = byId(controller.model(), childId);
    Column parentPk = makeColumn("11000000-0000-0000-0000-000000000001", "id");
    parentPk.primaryKey = true;
    parent->columns.append(parentPk);
    QVERIFY(child->columns.isEmpty());

    // No explicit columns: the child FK "customer_id" is created automatically.
    Relationship *relationship =
        controller.addRelationshipPick(RelationshipKind::OneToMany, childId, Id(), parentId, Id());
    QVERIFY(relationship);
    QCOMPARE(child->columns.size(), 1);
    const Column &created = child->columns.first();
    QCOMPARE(created.name, QStringLiteral("customer_id"));
    QVERIFY(created.nullable);
    QVERIFY(created.type == parentPk.type);
    QCOMPARE(relationship->parentColumns, QList<Id>{parentPk.id});
    QCOMPARE(controller.model()->relationships.size(), 1);
}

void DocumentControllerTest::addRelationshipPick_reusesExistingColumn()
{
    DocumentController controller;
    const Id parentId = controller.addEntity(QStringLiteral("customer"))->id;
    const Id childId = controller.addEntity(QStringLiteral("orders"))->id;

    Entity *parent = byId(controller.model(), parentId);
    Entity *child = byId(controller.model(), childId);
    Column parentPk = makeColumn("12000000-0000-0000-0000-000000000001", "id");
    parentPk.primaryKey = true;
    Column existingFk = makeColumn("12000000-0000-0000-0000-000000000002", "customer_id");
    parent->columns.append(parentPk);
    child->columns.append(existingFk);

    // A column named "customer_id" already exists: it is reused, not duplicated.
    QVERIFY(controller.addRelationshipPick(RelationshipKind::OneToMany, childId, Id(), parentId,
                                           Id()));
    QCOMPARE(child->columns.size(), 1);
    QCOMPARE(controller.model()->relationships.size(), 1);
}

QTEST_APPLESS_MAIN(DocumentControllerTest)

#include "tst_document.moc"