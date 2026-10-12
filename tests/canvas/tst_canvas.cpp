#include "DiagramScene.h"
#include "DocumentController.h"
#include "EntityItem.h"

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

QTEST_MAIN(CanvasTest)

#include "tst_canvas.moc"