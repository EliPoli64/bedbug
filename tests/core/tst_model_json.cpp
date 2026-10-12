#include "JsonSerializer.h"
#include "Model.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QtTest>

namespace {

using namespace bb;

const char *const kIdCustomer    = "aa000000-0000-0000-0000-000000000001";
const char *const kIdOrders      = "aa000000-0000-0000-0000-000000000002";
const char *const kIdCId         = "aa000000-0000-0000-0000-000000000101";
const char *const kIdCEmail      = "aa000000-0000-0000-0000-000000000102";
const char *const kIdCStatus     = "aa000000-0000-0000-0000-000000000103";
const char *const kIdCBalance    = "aa000000-0000-0000-0000-000000000104";
const char *const kIdCData       = "aa000000-0000-0000-0000-000000000105";
const char *const kIdCPublicId   = "aa000000-0000-0000-0000-000000000106";
const char *const kIdCBio        = "aa000000-0000-0000-0000-000000000107";
const char *const kIdCCreatedAt  = "aa000000-0000-0000-0000-000000000108";
const char *const kIdOId         = "aa000000-0000-0000-0000-000000000201";
const char *const kIdOCustomerId = "aa000000-0000-0000-0000-000000000202";
const char *const kIdOTotal      = "aa000000-0000-0000-0000-000000000203";
const char *const kIdIndex       = "aa000000-0000-0000-0000-000000000301";
const char *const kIdRel         = "aa000000-0000-0000-0000-000000000401";
const char *const kIdDMain       = "aa000000-0000-0000-0000-000000000501";
const char *const kIdDTopo       = "aa000000-0000-0000-0000-000000000502";

Id makeId(const char *text)
{
    bool ok = false;
    const Id id = Id::fromString(QLatin1String(text), &ok);
    Q_ASSERT(ok);
    return id;
}

CanonicalType ct(CanonicalType::Kind kind)
{
    CanonicalType t;
    t.kind = kind;
    return t;
}

CanonicalType ctVarchar(int length)
{
    CanonicalType t;
    t.kind = CanonicalType::Kind::Varchar;
    t.length = length;
    return t;
}

CanonicalType ctDecimal(int precision, int scale)
{
    CanonicalType t;
    t.kind = CanonicalType::Kind::Decimal;
    t.precision = precision;
    t.scale = scale;
    return t;
}

CanonicalType ctEnum(const QStringList &values)
{
    CanonicalType t;
    t.kind = CanonicalType::Kind::Enum;
    t.enumValues = values;
    return t;
}

// Mirrors tests/fixtures/sample_model.json. Keep both in sync.
Model buildFixtureModel()
{
    Model model;
    model.name = QStringLiteral("Sales");
    model.preferredDialect = QStringLiteral("postgres");

    Entity customer;
    customer.id = makeId(kIdCustomer);
    customer.name = QStringLiteral("customer");
    customer.schema = QStringLiteral("public");
    customer.comment = QStringLiteral("Customer master data.");

    Column id;
    id.id = makeId(kIdCId);
    id.name = QStringLiteral("id");
    id.type = ct(CanonicalType::Kind::BigInt);
    id.nullable = false;
    id.primaryKey = true;
    id.autoIncrement = true;

    Column email;
    email.id = makeId(kIdCEmail);
    email.name = QStringLiteral("email");
    email.type = ctVarchar(255);
    email.nullable = false;
    email.unique = true;

    Column status;
    status.id = makeId(kIdCStatus);
    status.name = QStringLiteral("status");
    status.type = ctEnum({QStringLiteral("active"), QStringLiteral("inactive"), QStringLiteral("banned")});
    status.nullable = false;
    status.defaultValue = QStringLiteral("'active'");
    status.comment = QStringLiteral("Account lifecycle state.");

    Column balance;
    balance.id = makeId(kIdCBalance);
    balance.name = QStringLiteral("balance");
    balance.type = ctDecimal(10, 2);
    balance.nullable = true;

    Column data;
    data.id = makeId(kIdCData);
    data.name = QStringLiteral("data");
    data.type = ct(CanonicalType::Kind::Json);
    data.nullable = true;
    data.dialectOverrides.insert(QStringLiteral("sqlserver"), QStringLiteral("nvarchar(max)"));
    data.nativeType = DialectHint{QStringLiteral("jsonb"), QStringLiteral("postgres")};

    Column publicId;
    publicId.id = makeId(kIdCPublicId);
    publicId.name = QStringLiteral("public_id");
    publicId.type = ct(CanonicalType::Kind::Uuid);
    publicId.nullable = true;

    Column bio;
    bio.id = makeId(kIdCBio);
    bio.name = QStringLiteral("bio");
    bio.type = ct(CanonicalType::Kind::Text);
    bio.nullable = true;

    Column createdAt;
    createdAt.id = makeId(kIdCCreatedAt);
    createdAt.name = QStringLiteral("created_at");
    createdAt.type = ct(CanonicalType::Kind::Timestamp);
    createdAt.nullable = false;
    createdAt.defaultValue = QStringLiteral("CURRENT_TIMESTAMP");

    customer.columns = {id, email, status, balance, data, publicId, bio, createdAt};

    Index ixCreated;
    ixCreated.id = makeId(kIdIndex);
    ixCreated.name = QStringLiteral("ix_customer_created_at");
    ixCreated.columnIds = {makeId(kIdCCreatedAt)};
    customer.indexes = {ixCreated};

    Entity orders;
    orders.id = makeId(kIdOrders);
    orders.name = QStringLiteral("orders");
    orders.schema = QStringLiteral("public");

    Column orderId;
    orderId.id = makeId(kIdOId);
    orderId.name = QStringLiteral("id");
    orderId.type = ct(CanonicalType::Kind::BigInt);
    orderId.nullable = false;
    orderId.primaryKey = true;
    orderId.autoIncrement = true;

    Column customerId;
    customerId.id = makeId(kIdOCustomerId);
    customerId.name = QStringLiteral("customer_id");
    customerId.type = ct(CanonicalType::Kind::BigInt);
    customerId.nullable = false;

    Column total;
    total.id = makeId(kIdOTotal);
    total.name = QStringLiteral("total");
    total.type = ctDecimal(12, 2);
    total.nullable = false;
    total.defaultValue = QStringLiteral("0");

    orders.columns = {orderId, customerId, total};

    Relationship rel;
    rel.id = makeId(kIdRel);
    rel.name = QStringLiteral("fk_orders_customer");
    rel.childEntity = makeId(kIdOrders);
    rel.childColumns = {makeId(kIdOCustomerId)};
    rel.parentEntity = makeId(kIdCustomer);
    rel.parentColumns = {makeId(kIdCId)};
    rel.onDelete = QStringLiteral("CASCADE");
    rel.onUpdate = QStringLiteral("RESTRICT");

    model.entities = {customer, orders};
    model.relationships = {rel};

    Diagram main;
    main.id = makeId(kIdDMain);
    main.name = QStringLiteral("Main");
    main.nodes = {DiagramNode{makeId(kIdCustomer), QPointF(40, 80)},
                  DiagramNode{makeId(kIdOrders), QPointF(320, 80)}};

    Diagram topology;
    topology.id = makeId(kIdDTopo);
    topology.name = QStringLiteral("Topology");
    topology.nodes = {DiagramNode{makeId(kIdOrders), QPointF(10, 10)}};
    topology.zoom = 1.5;

    model.diagrams = {main, topology};

    return model;
}

} // namespace

class ModelJsonTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultModel_roundTrips();
    void fixtureModel_roundTrips();
    void fixtureFile_parses_and_matches();
    void rejects_unknownFormatVersion();
    void rejects_missingFormatVersion();
    void rejects_entityWithoutName();
    void rejects_unknownTypeKind();
    void canonicalType_allKinds_compare();
    void canonicalType_kindConversion();
    void canonicalType_validation();
    void id_roundTrip();
    void domainHelpers();
};

void ModelJsonTest::defaultModel_roundTrips()
{
    const Model original;
    const Model loaded = JsonSerializer::fromJson(JsonSerializer::toJson(original));
    QCOMPARE(loaded, original);
    QCOMPARE(loaded.formatVersion, Model::currentFormatVersion());
    QVERIFY(loaded.entities.isEmpty());
    QVERIFY(loaded.diagrams.isEmpty());
}

void ModelJsonTest::fixtureModel_roundTrips()
{
    const Model original = buildFixtureModel();
    const Model loaded = JsonSerializer::fromJson(JsonSerializer::toJson(original));
    QCOMPARE(loaded, original);
    QCOMPARE(loaded.entities.size(), 2);
    QCOMPARE(loaded.relationships.size(), 1);
    QCOMPARE(loaded.diagrams.size(), 2);
}

void ModelJsonTest::fixtureFile_parses_and_matches()
{
    const QString path = QFileInfo(QString::fromLatin1(__FILE__))
                             .dir()
                             .filePath(QStringLiteral("../fixtures/sample_model.json"));
    QFile file(path);
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QVERIFY2(!doc.isNull(), "fixture file must be valid JSON");
    QVERIFY(doc.isObject());

    QString error;
    const Model parsed = JsonSerializer::fromJson(doc.object(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(parsed, buildFixtureModel());
}

void ModelJsonTest::rejects_unknownFormatVersion()
{
    QJsonObject root;
    root.insert(QStringLiteral("formatVersion"), QStringLiteral("2"));
    QString error;
    const Model model = JsonSerializer::fromJson(root, &error);
    QVERIFY2(error.contains(QStringLiteral("unsupported format version")), qPrintable(error));
    QVERIFY(model.entities.isEmpty());
}

void ModelJsonTest::rejects_missingFormatVersion()
{
    QJsonObject root;
    root.insert(QStringLiteral("entities"), QJsonArray());
    QString error;
    (void) JsonSerializer::fromJson(root, &error);
    QVERIFY2(error.contains(QStringLiteral("formatVersion")), qPrintable(error));
}

void ModelJsonTest::rejects_entityWithoutName()
{
    QJsonObject root;
    root.insert(QStringLiteral("formatVersion"), Model::currentFormatVersion());

    QJsonObject entity;
    entity.insert(QStringLiteral("id"), QStringLiteral("aa000000-0000-0000-0000-000000000001"));
    entity.insert(QStringLiteral("columns"), QJsonArray());
    entity.insert(QStringLiteral("indexes"), QJsonArray());

    QJsonArray entities;
    entities.append(entity);
    root.insert(QStringLiteral("entities"), entities);

    QString error;
    (void) JsonSerializer::fromJson(root, &error);
    QVERIFY2(error.contains(QStringLiteral("name")), qPrintable(error));
}

void ModelJsonTest::rejects_unknownTypeKind()
{
    QJsonObject kind;
    kind.insert(QStringLiteral("kind"), QStringLiteral("Money"));

    QJsonObject column;
    column.insert(QStringLiteral("id"), QStringLiteral("aa000000-0000-0000-0000-000000000101"));
    column.insert(QStringLiteral("name"), QStringLiteral("salary"));
    column.insert(QStringLiteral("type"), kind);

    QJsonArray columns;
    columns.append(column);

    QJsonObject entity;
    entity.insert(QStringLiteral("id"), QStringLiteral("aa000000-0000-0000-0000-000000000001"));
    entity.insert(QStringLiteral("name"), QStringLiteral("employee"));
    entity.insert(QStringLiteral("columns"), columns);
    entity.insert(QStringLiteral("indexes"), QJsonArray());

    QJsonArray entities;
    entities.append(entity);

    QJsonObject root;
    root.insert(QStringLiteral("formatVersion"), Model::currentFormatVersion());
    root.insert(QStringLiteral("entities"), entities);

    QString error;
    (void) JsonSerializer::fromJson(root, &error);
    QVERIFY2(error.contains(QStringLiteral("unknown type kind")), qPrintable(error));
}

void ModelJsonTest::canonicalType_allKinds_compare()
{
    const auto kinds = {CanonicalType::Kind::Int,       CanonicalType::Kind::BigInt,
                        CanonicalType::Kind::SmallInt,  CanonicalType::Kind::Varchar,
                        CanonicalType::Kind::Char,      CanonicalType::Kind::Text,
                        CanonicalType::Kind::Boolean,   CanonicalType::Kind::Date,
                        CanonicalType::Kind::Time,      CanonicalType::Kind::Timestamp,
                        CanonicalType::Kind::Decimal,   CanonicalType::Kind::Float,
                        CanonicalType::Kind::Double,    CanonicalType::Kind::Blob,
                        CanonicalType::Kind::Json,      CanonicalType::Kind::Uuid,
                        CanonicalType::Kind::Enum};

    for (const CanonicalType::Kind kind : kinds) {
        const CanonicalType a = ct(kind);
        const CanonicalType b = ct(kind);
        QVERIFY(a == b);
        QVERIFY(!(a != b));
        QVERIFY(a == a);
    }
}

void ModelJsonTest::canonicalType_kindConversion()
{
    const auto kinds = {CanonicalType::Kind::Int,       CanonicalType::Kind::BigInt,
                        CanonicalType::Kind::SmallInt,  CanonicalType::Kind::Varchar,
                        CanonicalType::Kind::Char,      CanonicalType::Kind::Text,
                        CanonicalType::Kind::Boolean,   CanonicalType::Kind::Date,
                        CanonicalType::Kind::Time,      CanonicalType::Kind::Timestamp,
                        CanonicalType::Kind::Decimal,   CanonicalType::Kind::Float,
                        CanonicalType::Kind::Double,    CanonicalType::Kind::Blob,
                        CanonicalType::Kind::Json,      CanonicalType::Kind::Uuid,
                        CanonicalType::Kind::Enum};

    for (const CanonicalType::Kind kind : kinds) {
        const QString name = CanonicalType::kindToString(kind);
        QVERIFY(!name.isEmpty());
        bool ok = false;
        const CanonicalType::Kind back = CanonicalType::stringToKind(name, &ok);
        QVERIFY(ok);
        QVERIFY(back == kind);
    }

    bool ok = true;
    const CanonicalType::Kind unknown = CanonicalType::stringToKind(QStringLiteral("Money"), &ok);
    QVERIFY(!ok);
    QVERIFY(unknown == CanonicalType::Kind::Varchar);
}

void ModelJsonTest::canonicalType_validation()
{
    CanonicalType varchar = ctVarchar(255);
    QVERIFY(varchar.isValid());

    CanonicalType badVarchar = ct(CanonicalType::Kind::Varchar);
    badVarchar.length = 0;
    QVERIFY(!badVarchar.isValid());

    CanonicalType decimal = ctDecimal(10, 2);
    QVERIFY(decimal.isValid());

    CanonicalType badDecimal = ctDecimal(0, 5);
    QVERIFY(!badDecimal.isValid());

    CanonicalType badScale = ctDecimal(5, 9);
    QVERIFY(!badScale.isValid());

    CanonicalType emptyEnum = ctEnum({});
    QVERIFY(!emptyEnum.isValid());

    CanonicalType goodEnum = ctEnum({QStringLiteral("a"), QStringLiteral("b")});
    QVERIFY(goodEnum.isValid());

    CanonicalType uuid = ct(CanonicalType::Kind::Uuid);
    QVERIFY(uuid.isValid());
}

void ModelJsonTest::id_roundTrip()
{
    const Id id = Id::create();
    QVERIFY(!id.isNull());
    QVERIFY(!id.toString().isEmpty());

    bool ok = false;
    const Id parsed = Id::fromString(id.toString(), &ok);
    QVERIFY(ok);
    QVERIFY(parsed == id);
    QVERIFY(!(parsed != id));

    const Id invalid = Id::fromString(QStringLiteral("not-a-uuid"), &ok);
    QVERIFY(!ok);
    QVERIFY(invalid.isNull());

    const Id nullId;
    QVERIFY(nullId.isNull());
}

void ModelJsonTest::domainHelpers()
{
    const Model model = buildFixtureModel();

    const Entity *customer = model.findEntity(makeId(kIdCustomer));
    QVERIFY(customer);
    QVERIFY(!model.findEntity(Id::create()));

    const Column *email = customer->findColumn(makeId(kIdCEmail));
    QVERIFY(email);
    QCOMPARE(email->name, QStringLiteral("email"));
    QVERIFY(!customer->findColumn(Id::create()));

    QCOMPARE(model.relationshipsFor(makeId(kIdOrders)).size(), 1);
    QCOMPARE(model.relationshipsFor(makeId(kIdCustomer)).size(), 1);
    QVERIFY(model.relationshipsFor(Id::create()).isEmpty());
}

QTEST_APPLESS_MAIN(ModelJsonTest)

#include "tst_model_json.moc"