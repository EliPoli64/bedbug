#include "CrowsFootPainter.h"
#include "DiagramScene.h"
#include "DocumentController.h"
#include "EntityItem.h"
#include "RelationshipItem.h"

#include <QImage>
#include <QPainter>
#include <QtMath>
#include <QtTest>

namespace {

using namespace bb;

// A two-entity scene model with diagram nodes at known positions.
Model twoEntitySceneModel()
{
    Model model;

    Entity customer;
    customer.id = Id::create();
    customer.name = QStringLiteral("customer");

    Column id;
    id.id = Id::create();
    id.name = QStringLiteral("id");
    id.type.kind = CanonicalType::Kind::BigInt;
    id.nullable = false;
    id.primaryKey = true;
    id.autoIncrement = true;

    Column email;
    email.id = Id::create();
    email.name = QStringLiteral("email");
    email.type.kind = CanonicalType::Kind::Varchar;
    email.type.length = 255;
    email.nullable = false;
    email.unique = true;

    customer.columns = {id, email};

    Entity orders;
    orders.id = Id::create();
    orders.name = QStringLiteral("orders");
    orders.columns = {id, email};   // shared column ids are not realistic; re-scope below

    // Rebuild orders with its own columns.
    Column orderId;
    orderId.id = Id::create();
    orderId.name = QStringLiteral("id");
    orderId.type.kind = CanonicalType::Kind::BigInt;
    orderId.nullable = false;
    orderId.primaryKey = true;
    Column customerRef;
    customerRef.id = Id::create();
    customerRef.name = QStringLiteral("customer_id");
    customerRef.type.kind = CanonicalType::Kind::BigInt;
    orders.columns = {orderId, customerRef};

    Diagram diagram;
    diagram.id = Id::create();
    diagram.name = QStringLiteral("Main");
    diagram.nodes = {DiagramNode{customer.id, QPointF(40, 80)},
                     DiagramNode{orders.id, QPointF(320, 80)}};

    model.entities = {customer, orders};
    model.diagrams = {diagram};
    return model;
}

const Column *columnByName(const Entity &entity, const QString &name)
{
    for (const Column &column : entity.columns) {
        if (column.name == name)
            return &column;
    }
    return nullptr;
}

// TwoEntitySceneModel with one FK: orders.customer_id -> customer.id.
Model twoEntityConnectedModel()
{
    Model model = twoEntitySceneModel();
    const Entity &customer = model.entities.at(0);
    const Entity &orders = model.entities.at(1);

    const Column *pk = columnByName(customer, QStringLiteral("id"));
    const Column *fk = columnByName(orders, QStringLiteral("customer_id"));
    Q_ASSERT(pk && fk);

    Relationship relationship;
    relationship.id = Id::create();
    relationship.name = QStringLiteral("fk_orders_customer");
    relationship.childEntity = orders.id;
    relationship.childColumns = {fk->id};
    relationship.parentEntity = customer.id;
    relationship.parentColumns = {pk->id};
    model.relationships = {relationship};
    return model;
}

} // namespace

class CanvasTest : public QObject
{
    Q_OBJECT

private slots:
    void syncFromModel_createsItems();
    void moveItem_updatesDiagramNode();
    void snapToGrid_roundsPosition();
    void columnHitTest();
    void ports_areOnRowCenters();
    void addEntityViaController_updatesScene();
    void paint_rendersEntityWithoutError();
    void relationshipItem_rendersAndRefreshesOnMove();
    void relationshipItem_skipped_whenEntityNotOnDiagram();
    void linkMode_clicksCreateRelationship();
    void linkTool_oneToOneFlowsThrough();
    void tableClicks_resolveColumnsViaHandler();
    void relationshipItem_cardinalityDerivedFromUniqueness();
    void contextMenu_onTableEmitsRequest();
    void crowsfoot_markersRenderDistinctly();
};

void CanvasTest::syncFromModel_createsItems()
{
    Model *model = new Model(twoEntitySceneModel());
    DiagramScene scene;
    scene.setModel(model);
    scene.setActiveDiagramId(model->diagrams.first().id);
    scene.syncFromModel();

    const Id customerId = model->entities.constFirst().id;
    const Id ordersId = model->entities.at(1).id;

    EntityItem *customerItem = scene.itemForEntity(customerId);
    EntityItem *ordersItem = scene.itemForEntity(ordersId);
    QVERIFY(customerItem);
    QVERIFY(ordersItem);
    QCOMPARE(customerItem->entityId(), customerId);

    QVERIFY(customerItem->boundingRect().width() > 100);
    QVERIFY(customerItem->boundingRect().height() > EntityItem::kHeaderHeight);

    // Positions match the diagram nodes.
    QCOMPARE(customerItem->pos(), model->diagrams.first().nodes.at(0).pos);

    delete model;
}

void CanvasTest::moveItem_updatesDiagramNode()
{
    Model *model = new Model(twoEntitySceneModel());
    DiagramScene scene;
    scene.setModel(model);
    scene.setActiveDiagramId(model->diagrams.first().id);
    scene.setSnapToGrid(false);   // isolate the move from snapping
    scene.syncFromModel();

    const Id customerId = model->entities.constFirst().id;
    EntityItem *item = scene.itemForEntity(customerId);
    QVERIFY(item);

    item->setPos(QPointF(150, 200));
    QCOMPARE(scene.currentDiagram()->nodes.constFirst().pos, QPointF(150, 200));

    delete model;
}

void CanvasTest::snapToGrid_roundsPosition()
{
    Model *model = new Model(twoEntitySceneModel());
    DiagramScene scene;
    scene.setModel(model);
    scene.setActiveDiagramId(model->diagrams.first().id);
    scene.syncFromModel();
    scene.setSnapToGrid(true);   // the default, made explicit

    const Id customerId = model->entities.constFirst().id;
    EntityItem *item = scene.itemForEntity(customerId);

    item->setPos(QPointF(13.0, 31.0));
    // round(13/20) = 20, round(31/20) = 40
    QCOMPARE(scene.currentDiagram()->nodes.constFirst().pos, QPointF(20, 40));

    delete model;
}

void CanvasTest::columnHitTest()
{
    Model *model = new Model(twoEntitySceneModel());
    DiagramScene scene;
    scene.setModel(model);
    scene.setActiveDiagramId(model->diagrams.first().id);
    scene.syncFromModel();

    EntityItem *item = scene.itemForEntity(model->entities.constFirst().id);
    QVERIFY(item);

    const qreal firstRowCenterY = EntityItem::kHeaderHeight + EntityItem::kRowHeight / 2.0;
    const qreal secondRowCenterY = EntityItem::kHeaderHeight + 1.5 * EntityItem::kRowHeight;

    QCOMPARE(item->columnAt(QPointF(10, firstRowCenterY)), 0);
    QCOMPARE(item->columnAt(QPointF(10, secondRowCenterY)), 1);
    QCOMPARE(item->columnAt(QPointF(10, EntityItem::kHeaderHeight / 2.0)), -1);   // header
    QCOMPARE(item->columnAt(QPointF(10, 9999)), -1);                              // below body

    delete model;
}

void CanvasTest::ports_areOnRowCenters()
{
    Model *model = new Model(twoEntitySceneModel());
    DiagramScene scene;
    scene.setModel(model);
    scene.setActiveDiagramId(model->diagrams.first().id);
    scene.syncFromModel();

    const Entity &customer = model->entities.constFirst();
    const Column &idColumn = customer.columns.constFirst();
    EntityItem *item = scene.itemForEntity(customer.id);
    QVERIFY(item);

    const QPointF left = item->leftPortFor(idColumn.id);
    const QPointF right = item->rightPortFor(idColumn.id);

    // The item sits at node pos (40, 80); the left port is exactly at its left edge.
    QCOMPARE(left.x(), item->pos().x());
    QVERIFY(qFuzzyCompare(left.y(), right.y()));
    QVERIFY(left.y() > EntityItem::kHeaderHeight);
    QVERIFY(right.x() > item->pos().x());

    // Unknown column yields a null point.
    QCOMPARE(item->leftPortFor(Id::create()), QPointF());

    delete model;
}

void CanvasTest::addEntityViaController_updatesScene()
{
    DocumentController controller;
    DiagramScene scene;
    scene.setModel(controller.model());
    scene.setActiveDiagramId(controller.activeDiagramId());
    QObject::connect(&controller, &DocumentController::modelChanged, &scene,
                     [&scene] { scene.syncFromModel(); });

    QVERIFY(controller.model()->entities.isEmpty());
    QVERIFY(scene.items().isEmpty());

    Entity *created = controller.addEntity(QStringLiteral("orders"));
    QVERIFY(created);

    EntityItem *item = scene.itemForEntity(created->id);
    QVERIFY(item);
    QCOMPARE(item->entity()->name, QStringLiteral("orders"));
    QCOMPARE(scene.currentDiagram()->nodes.size(), 1);
}

void CanvasTest::paint_rendersEntityWithoutError()
{
    Model *model = new Model(twoEntitySceneModel());
    DiagramScene scene;
    scene.setModel(model);
    scene.setActiveDiagramId(model->diagrams.first().id);
    scene.syncFromModel();

    const QRectF bounds = scene.itemsBoundingRect().adjusted(-10, -10, 10, 10);
    QVERIFY(!bounds.isEmpty());

    QImage image(qCeil(bounds.width()), qCeil(bounds.height()), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    scene.render(&painter, QRectF(image.rect()), bounds);
    painter.end();

    QVERIFY(!image.isNull());

    // At least one non-white pixel: the item actually painted something.
    bool painted = false;
    for (int y = 0; y < image.height() && !painted; ++y)
        for (int x = 0; x < image.width() && !painted; ++x)
            painted = image.pixel(x, y) != qRgba(255, 255, 255, 255);
    QVERIFY(painted);

    delete model;
}

void CanvasTest::relationshipItem_rendersAndRefreshesOnMove()
{
    Model *model = new Model(twoEntityConnectedModel());
    DiagramScene scene;
    scene.setModel(model);
    scene.setActiveDiagramId(model->diagrams.first().id);
    scene.setSnapToGrid(false);
    scene.syncFromModel();

    const Id relationshipId = model->relationships.first().id;
    RelationshipItem *item = scene.itemForRelationship(relationshipId);
    QVERIFY(item);
    QVERIFY(item->isVisible());

    const QRectF before = item->boundingRect();
    QVERIFY(before.width() > 0);

    // Move the child across to the other side; the edge must re-route.
    const Id ordersId = model->entities.at(1).id;
    scene.itemForEntity(ordersId)->setPos(QPointF(700, 320));
    const QRectF after = item->boundingRect();
    QVERIFY(after != before);

    delete model;
}

void CanvasTest::relationshipItem_skipped_whenEntityNotOnDiagram()
{
    Model model = twoEntitySceneModel();

    // A third entity that has a relationship but sits on no diagram.
    Entity orphan;
    orphan.id = Id::create();
    orphan.name = QStringLiteral("orphan");
    const Id customerColumnId = columnByName(model.entities.constFirst(), QStringLiteral("id"))->id;
    Column orphanId;
    orphanId.id = Id::create();
    orphanId.name = QStringLiteral("id");
    orphan.columns = {orphanId};

    Relationship stray;
    stray.id = Id::create();
    stray.name = QStringLiteral("fk_customer_orphan");
    stray.childEntity = model.entities.constFirst().id;
    stray.childColumns = {customerColumnId};
    stray.parentEntity = orphan.id;
    stray.parentColumns = {orphanId.id};
    model.relationships = {stray};

    DiagramScene scene;
    scene.setModel(&model);
    scene.setActiveDiagramId(model.diagrams.first().id);
    scene.syncFromModel();

    // The edge references an entity that has no item: no relationship item is created.
    QCOMPARE(scene.itemForRelationship(stray.id), nullptr);
}

void CanvasTest::linkMode_clicksCreateRelationship()
{
    Model *model = new Model(twoEntitySceneModel());
    DiagramScene scene;
    scene.setModel(model);
    scene.setActiveDiagramId(model->diagrams.first().id);
    scene.setSnapToGrid(false);
    scene.syncFromModel();

    RelationshipKind createdKind = RelationshipKind::OneToMany;
    bool called = false;
    Id childId, parentId;
    QList<Id> childColumns, parentColumns;
    scene.setRelationshipCreationHandler(
        [&](const Id &child, const QList<Id> &childCols, const Id &parent,
            const QList<Id> &parentCols, RelationshipKind kind) -> bool {
            called = true;
            childId = child;
            childColumns = childCols;
            parentId = parent;
            parentColumns = parentCols;
            createdKind = kind;
            return true;
        });

    scene.setLinkTool(std::make_optional(RelationshipKind::OneToMany));
    QVERIFY(scene.linkTool().has_value());
    QVERIFY(*scene.linkTool() == RelationshipKind::OneToMany);

    const Entity &customer = model->entities.at(0);
    const Entity &orders = model->entities.at(1);
    const Column *pk = columnByName(customer, QStringLiteral("id"));
    const Column *fk = columnByName(orders, QStringLiteral("customer_id"));

    EntityItem *ordersItem = scene.itemForEntity(orders.id);
    EntityItem *customerItem = scene.itemForEntity(customer.id);

    // Click the inside of the FK row (orders.customer_id, second row).
    const qreal childRowY = EntityItem::kHeaderHeight + 1 * EntityItem::kRowHeight
                            + EntityItem::kRowHeight / 2.0;
    const QPointF childClick = ordersItem->mapToScene(QPointF(10, childRowY));
    QVERIFY(scene.handleLinkAt(childClick));
    QVERIFY(!called);

    // Click the inside of the PK row (customer.id, first row).
    const qreal parentRowY = EntityItem::kHeaderHeight + EntityItem::kRowHeight / 2.0;
    const QPointF parentClick = customerItem->mapToScene(QPointF(10, parentRowY));
    QVERIFY(scene.handleLinkAt(parentClick));
    QVERIFY(called);
    QCOMPARE(childId, orders.id);
    QCOMPARE(childColumns, QList<Id>{fk->id});
    QCOMPARE(parentId, customer.id);
    QCOMPARE(parentColumns, QList<Id>{pk->id});
    QVERIFY(createdKind == RelationshipKind::OneToMany);

    delete model;
}

void CanvasTest::linkTool_oneToOneFlowsThrough()
{
    Model *model = new Model(twoEntitySceneModel());
    DiagramScene scene;
    scene.setModel(model);
    scene.setActiveDiagramId(model->diagrams.first().id);
    scene.syncFromModel();

    RelationshipKind createdKind = RelationshipKind::OneToMany;
    scene.setRelationshipCreationHandler(
        [&](const Id &, const QList<Id> &, const Id &, const QList<Id> &,
            RelationshipKind kind) -> bool {
            createdKind = kind;
            return true;
        });

    scene.setLinkTool(std::make_optional(RelationshipKind::OneToOne));
    const Entity &customer = model->entities.at(0);
    const Entity &orders = model->entities.at(1);
    EntityItem *ordersItem = scene.itemForEntity(orders.id);
    EntityItem *customerItem = scene.itemForEntity(customer.id);

    const qreal childRowY = EntityItem::kHeaderHeight + 1 * EntityItem::kRowHeight
                            + EntityItem::kRowHeight / 2.0;
    const qreal parentRowY = EntityItem::kHeaderHeight + EntityItem::kRowHeight / 2.0;
    scene.handleLinkAt(ordersItem->mapToScene(QPointF(10, childRowY)));
    scene.handleLinkAt(customerItem->mapToScene(QPointF(10, parentRowY)));
    QVERIFY(createdKind == RelationshipKind::OneToOne);

    scene.setLinkTool(std::nullopt);
    QVERIFY(!scene.linkTool().has_value());

    delete model;
}

void CanvasTest::tableClicks_resolveColumnsViaHandler()
{
    Model *model = new Model(twoEntitySceneModel());
    DiagramScene scene;
    scene.setModel(model);
    scene.setActiveDiagramId(model->diagrams.first().id);
    scene.syncFromModel();

    bool called = false;
    QList<Id> childColumns, parentColumns;
    scene.setRelationshipCreationHandler(
        [&](const Id &, const QList<Id> &childCols, const Id &, const QList<Id> &parentCols,
            RelationshipKind) -> bool {
            called = true;
            childColumns = childCols;
            parentColumns = parentCols;
            return true;
        });

    scene.setLinkTool(std::make_optional(RelationshipKind::OneToMany));

    const Entity &customer = model->entities.at(0);
    const Entity &orders = model->entities.at(1);
    EntityItem *ordersItem = scene.itemForEntity(orders.id);
    EntityItem *customerItem = scene.itemForEntity(customer.id);

    // Click the child's HEADER and the parent's HEADER: no specific columns — the handler
    // receives empty lists so the shell resolves/creates columns automatically.
    const QPointF childHeader(
        ordersItem->mapToScene(QPointF(20, EntityItem::kHeaderHeight / 2.0)));
    scene.handleLinkAt(childHeader);
    QVERIFY(!called);

    const QPointF parentHeader(
        customerItem->mapToScene(QPointF(20, EntityItem::kHeaderHeight / 2.0)));
    scene.handleLinkAt(parentHeader);
    QVERIFY(called);
    QVERIFY(childColumns.isEmpty());
    QVERIFY(parentColumns.isEmpty());

    delete model;
}

void CanvasTest::relationshipItem_cardinalityDerivedFromUniqueness()
{
    Model model = twoEntityConnectedModel();
    DiagramScene scene;
    scene.setModel(&model);
    scene.setActiveDiagramId(model.diagrams.first().id);
    scene.syncFromModel();

    const Id relationshipId = model.relationships.first().id;
    RelationshipItem *item = scene.itemForRelationship(relationshipId);
    QVERIFY(item);

    // customer_id is not unique -> crow's foot at the child end.
    QVERIFY(item->childCardinality() == Cardinality::Many);

    // Make it unique (1:1) -> both ends render as "one".
    const Relationship &relationship = model.relationships.first();
    const Id fkColumnId = relationship.childColumns.first();
    model.findEntity(relationship.childEntity)->findColumn(fkColumnId)->unique = true;
    item->refresh();
    QVERIFY(item->childCardinality() == Cardinality::One);

    // Toggling pointed lines must not break refresh.
    scene.setPointedLines(true);
    item->refresh();
    QVERIFY(item->isVisible());
}

void CanvasTest::contextMenu_onTableEmitsRequest()
{
    Model *model = new Model(twoEntitySceneModel());
    DiagramScene scene;
    scene.setModel(model);
    scene.setActiveDiagramId(model->diagrams.first().id);
    scene.syncFromModel();

    const Id entityId = model->entities.constFirst().id;
    EntityItem *item = scene.itemForEntity(entityId);
    QVERIFY(item);

    bool emitted = false;
    Id requestedId;
    QPoint requestedPos;
    connect(&scene, &DiagramScene::tableContextMenuRequested, this,
            [&](const Id &id, const QPoint &pos) {
                emitted = true;
                requestedId = id;
                requestedPos = pos;
            });

    const QPointF scenePoint = item->mapToScene(item->boundingRect().center());
    QVERIFY(scene.handleTableContextMenu(scenePoint, QPoint(42, 43)));
    QVERIFY(emitted);
    QCOMPARE(requestedId, entityId);
    QCOMPARE(requestedPos, QPoint(42, 43));

    // Empty space: not handled, no signal.
    emitted = false;
    QVERIFY(!scene.handleTableContextMenu(QPointF(9000, 9000), QPoint()));
    QVERIFY(!emitted);

    delete model;
}

void CanvasTest::crowsfoot_markersRenderDistinctly()
{
    auto render = [](Cardinality cardinality, bool optional) {
        QImage image(96, 64, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.setPen(QPen(Qt::black, 1));
        CrowsFootPainter::paint(&painter, QPointF(20, 32), QPointF(1, 0), cardinality, optional);
        painter.end();
        return image;
    };
    auto inkCount = [](const QImage &image) {
        int ink = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (qAlpha(image.pixel(x, y)) > 0 && qGray(image.pixel(x, y)) < 200)
                    ++ink;
        return ink;
    };

    const QImage many = render(Cardinality::Many, false);
    const QImage one = render(Cardinality::One, false);
    const QImage manyOptional = render(Cardinality::Many, true);

    QVERIFY(inkCount(many) > 0);
    QVERIFY(inkCount(many) != inkCount(one));
    QVERIFY(inkCount(manyOptional) > inkCount(many));
}

QTEST_MAIN(CanvasTest)

#include "tst_canvas.moc"