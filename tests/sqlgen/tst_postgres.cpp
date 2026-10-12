#include "SqlGenerator.h"
#include "dialects/Postgres.h"

#include <QtTest>

namespace {

using namespace bb;

const PostgresDialect kPostgres;

Id mid(const char *text)
{
    bool ok = false;
    const Id id = Id::fromString(QLatin1String(text), &ok);
    Q_ASSERT(ok);
    return id;
}

Entity makeEntity(const char *idText, const QString &name, const QString &schema)
{
    Entity entity;
    entity.id = mid(idText);
    entity.name = name;
    entity.schema = schema;
    return entity;
}

Column makeColumn(const char *idText, const QString &name, CanonicalType::Kind kind)
{
    Column column;
    column.id = mid(idText);
    column.name = name;
    column.type.kind = kind;
    return column;
}

// A parent B-table "parent" (PK id) and a child "child" (FK parent_id).
Model twoTablesModel()
{
    Model model;

    Entity parent = makeEntity("10000000-0000-0000-0000-000000000001", "parent", "public");
    Column parentId = makeColumn("10000000-0000-0000-0000-000000000101", "id",
                                 CanonicalType::Kind::BigInt);
    parentId.nullable = false;
    parentId.primaryKey = true;
    parent.columns = {parentId};

    Entity child = makeEntity("10000000-0000-0000-0000-000000000002", "child", "public");
    Column childId = makeColumn("10000000-0000-0000-0000-000000000201", "id",
                                CanonicalType::Kind::BigInt);
    childId.nullable = false;
    childId.primaryKey = true;
    Column parentFk = makeColumn("10000000-0000-0000-0000-000000000202", "parent_id",
                                 CanonicalType::Kind::BigInt);
    parentFk.nullable = false;
    child.columns = {childId, parentFk};

    Relationship rel;
    rel.id = mid("10000000-0000-0000-0000-000000000301");
    rel.name = QStringLiteral("fk_child_parent");
    rel.childEntity = child.id;
    rel.childColumns = {parentFk.id};
    rel.parentEntity = parent.id;
    rel.parentColumns = {parentId.id};
    rel.onDelete = QStringLiteral("CASCADE");

    model.entities = {parent, child};
    model.relationships = {rel};
    return model;
}

// a -> b -> c -> a
Model cyclicModel()
{
    Model model;

    Entity a = makeEntity("20000000-0000-0000-0000-000000000001", "a", QString());
    Column aId = makeColumn("20000000-0000-0000-0000-000000000101", "id",
                            CanonicalType::Kind::BigInt);
    aId.primaryKey = true;
    aId.nullable = false;
    Column aC = makeColumn("20000000-0000-0000-0000-000000000102", "c_id",
                           CanonicalType::Kind::BigInt);
    a.columns = {aId, aC};

    Entity b = makeEntity("20000000-0000-0000-0000-000000000002", "b", QString());
    Column bId = makeColumn("20000000-0000-0000-0000-000000000201", "id",
                            CanonicalType::Kind::BigInt);
    bId.primaryKey = true;
    bId.nullable = false;
    Column bA = makeColumn("20000000-0000-0000-0000-000000000202", "a_id",
                           CanonicalType::Kind::BigInt);
    b.columns = {bId, bA};

    Entity c = makeEntity("20000000-0000-0000-0000-000000000003", "c", QString());
    Column cId = makeColumn("20000000-0000-0000-0000-000000000301", "id",
                            CanonicalType::Kind::BigInt);
    cId.primaryKey = true;
    cId.nullable = false;
    Column cB = makeColumn("20000000-0000-0000-0000-000000000302", "b_id",
                           CanonicalType::Kind::BigInt);
    c.columns = {cId, cB};

    Relationship aToC;
    aToC.id = mid("20000000-0000-0000-0000-000000000401");
    aToC.name = QStringLiteral("fk_a_c");
    aToC.childEntity = a.id;
    aToC.childColumns = {aC.id};
    aToC.parentEntity = c.id;
    aToC.parentColumns = {cId.id};

    Relationship bToA;
    bToA.id = mid("20000000-0000-0000-0000-000000000402");
    bToA.name = QStringLiteral("fk_b_a");
    bToA.childEntity = b.id;
    bToA.childColumns = {bA.id};
    bToA.parentEntity = a.id;
    bToA.parentColumns = {aId.id};

    Relationship cToB;
    cToB.id = mid("20000000-0000-0000-0000-000000000403");
    cToB.name = QStringLiteral("fk_c_b");
    cToB.childEntity = c.id;
    cToB.childColumns = {cB.id};
    cToB.parentEntity = b.id;
    cToB.parentColumns = {bId.id};

    model.entities = {a, b, c};
    model.relationships = {aToC, bToA, cToB};
    return model;
}

} // namespace

class SqlGenPostgresTest : public QObject
{
    Q_OBJECT

private slots:
    void quoteIdent_escapesDoubleQuotes();
    void quoteLiteral_escapesSingleQuotes();
    void mapType_coversAllKinds();
    void mapType_narrowedWithoutLength();
    void columnDefinition_ordersModifiers();
    void enumTypeName_sanitizes();
    void preTableStatements_emitEnumTypes();
    void createOrder_parentsBeforeChildren();
    void createOrder_cycleAppendedAtEnd();
    void resolveType_overridePriority();
    void resolveType_nativeHintPriority();
    void resolveType_enumUsesNativeType();
    void createTable_multiLine();
    void addForeignKeyConstraint_actions();
    void dropTable_ifExists();
    void generate_simpleScript_orderedCorrectly();
    void generate_cyclicScript_noForwardReferences();
};

void SqlGenPostgresTest::quoteIdent_escapesDoubleQuotes()
{
    QCOMPARE(kPostgres.quoteIdent(QStringLiteral("customer")), QStringLiteral("\"customer\""));
    QCOMPARE(kPostgres.quoteIdent(QStringLiteral("weird\"name")), QStringLiteral("\"weird\"\"name\""));
    QCOMPARE(kPostgres.quoteIdent(QStringLiteral("")), QStringLiteral("\"\""));
}

void SqlGenPostgresTest::quoteLiteral_escapesSingleQuotes()
{
    QCOMPARE(kPostgres.quoteLiteral(QStringLiteral("plain")), QStringLiteral("'plain'"));
    QCOMPARE(kPostgres.quoteLiteral(QStringLiteral("O'Brien")), QStringLiteral("'O''Brien'"));
}

void SqlGenPostgresTest::mapType_coversAllKinds()
{
    struct Case
    {
        CanonicalType::Kind kind;
        const char *expected;
    };

    const Case cases[] = {
        {CanonicalType::Kind::Int, "integer"},       {CanonicalType::Kind::BigInt, "bigint"},
        {CanonicalType::Kind::SmallInt, "smallint"}, {CanonicalType::Kind::Text, "text"},
        {CanonicalType::Kind::Boolean, "boolean"},   {CanonicalType::Kind::Date, "date"},
        {CanonicalType::Kind::Time, "time"},         {CanonicalType::Kind::Timestamp, "timestamptz"},
        {CanonicalType::Kind::Float, "real"},        {CanonicalType::Kind::Double, "double precision"},
        {CanonicalType::Kind::Blob, "bytea"},        {CanonicalType::Kind::Json, "jsonb"},
        {CanonicalType::Kind::Uuid, "uuid"},
    };

    for (const Case &c : cases) {
        CanonicalType type;
        type.kind = c.kind;
        const TypeMapping mapping = kPostgres.mapType(type);
        QCOMPARE(mapping.sqlType, QString::fromLatin1(c.expected));
        QVERIFY(mapping.fidelity == Fidelity::Exact);
    }

    CanonicalType varchar;
    varchar.kind = CanonicalType::Kind::Varchar;
    varchar.length = 255;
    QCOMPARE(kPostgres.mapType(varchar).sqlType, QStringLiteral("varchar(255)"));

    CanonicalType chr;
    chr.kind = CanonicalType::Kind::Char;
    chr.length = 2;
    QCOMPARE(kPostgres.mapType(chr).sqlType, QStringLiteral("char(2)"));

    CanonicalType decimalType;
    decimalType.kind = CanonicalType::Kind::Decimal;
    decimalType.precision = 12;
    decimalType.scale = 3;
    QCOMPARE(kPostgres.mapType(decimalType).sqlType, QStringLiteral("numeric(12,3)"));
}

void SqlGenPostgresTest::mapType_narrowedWithoutLength()
{
    CanonicalType varchar;
    varchar.kind = CanonicalType::Kind::Varchar;
    varchar.length = 0;
    const TypeMapping mapping = kPostgres.mapType(varchar);
    QCOMPARE(mapping.sqlType, QStringLiteral("varchar"));
    QVERIFY(mapping.fidelity == Fidelity::Narrowed);
    QVERIFY(!mapping.suggestedOverride.isEmpty());
}

void SqlGenPostgresTest::columnDefinition_ordersModifiers()
{
    Column identity = makeColumn("30000000-0000-0000-0000-000000000001", "id",
                                 CanonicalType::Kind::BigInt);
    identity.nullable = false;
    identity.autoIncrement = true;
    QCOMPARE(kPostgres.columnDefinition(identity, QStringLiteral("bigint")),
             QStringLiteral("\"id\" bigint NOT NULL GENERATED BY DEFAULT AS IDENTITY"));

    Column uniqueEmail = makeColumn("30000000-0000-0000-0000-000000000002", "email",
                                    CanonicalType::Kind::Varchar);
    uniqueEmail.type.length = 255;
    uniqueEmail.unique = true;
    QCOMPARE(kPostgres.columnDefinition(uniqueEmail, QStringLiteral("varchar(255)")),
             QStringLiteral("\"email\" varchar(255) UNIQUE"));

    Column withDefault = makeColumn("30000000-0000-0000-0000-000000000003", "created_at",
                                    CanonicalType::Kind::Timestamp);
    withDefault.nullable = false;
    withDefault.defaultValue = QStringLiteral("CURRENT_TIMESTAMP");
    QCOMPARE(kPostgres.columnDefinition(withDefault, QStringLiteral("timestamptz")),
             QStringLiteral("\"created_at\" timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP"));
}

void SqlGenPostgresTest::enumTypeName_sanitizes()
{
    Entity entity = makeEntity("30000000-0000-0000-0000-000000000010", "Order Items", QString());
    Column status = makeColumn("30000000-0000-0000-0000-000000000011", "Type?",
                               CanonicalType::Kind::Enum);
    QCOMPARE(kPostgres.enumTypeName(entity, status), QStringLiteral("order_items_type__enum"));

    entity.name = QStringLiteral("customer");
    status.name = QStringLiteral("status");
    QCOMPARE(kPostgres.enumTypeName(entity, status), QStringLiteral("customer_status_enum"));

    Column plain = makeColumn("30000000-0000-0000-0000-000000000012", "name",
                              CanonicalType::Kind::Varchar);
    QVERIFY(kPostgres.enumTypeName(entity, plain).isEmpty());
}

void SqlGenPostgresTest::preTableStatements_emitEnumTypes()
{
    Entity entity = makeEntity("30000000-0000-0000-0000-000000000020", "payment", QString());
    Column status = makeColumn("30000000-0000-0000-0000-000000000021", "kind",
                               CanonicalType::Kind::Enum);
    status.type.enumValues = {QStringLiteral("cash"), QStringLiteral("card")};
    Column amount = makeColumn("30000000-0000-0000-0000-000000000022", "amount",
                               CanonicalType::Kind::Decimal);
    amount.type.precision = 10;
    amount.type.scale = 2;
    entity.columns = {status, amount};

    Model model;
    model.entities = {entity};

    const QStringList statements = kPostgres.preTableStatements(model);
    QCOMPARE(statements.size(), 1);
    QCOMPARE(statements.first(),
             QStringLiteral("CREATE TYPE \"payment_kind_enum\" AS ENUM ('cash', 'card');"));
}

void SqlGenPostgresTest::createOrder_parentsBeforeChildren()
{
    const Model model = twoTablesModel();
    const QList<Id> order = SqlGenerator::createOrder(model);
    QCOMPARE(order.size(), 2);
    QCOMPARE(order.at(0), model.entities.at(0).id);   // parent
    QCOMPARE(order.at(1), model.entities.at(1).id);   // child
}

void SqlGenPostgresTest::createOrder_cycleAppendedAtEnd()
{
    const Model model = cyclicModel();
    const QList<Id> order = SqlGenerator::createOrder(model);
    QCOMPARE(order.size(), 3);
    // All are in a cycle, so order falls back to model order.
    QCOMPARE(order.at(0), model.entities.at(0).id);
    QCOMPARE(order.at(1), model.entities.at(1).id);
    QCOMPARE(order.at(2), model.entities.at(2).id);
}

void SqlGenPostgresTest::resolveType_overridePriority()
{
    Entity entity = makeEntity("40000000-0000-0000-0000-000000000001", "e", QString());
    Column column = makeColumn("40000000-0000-0000-0000-000000000101", "c",
                               CanonicalType::Kind::Json);
    column.dialectOverrides.insert(QStringLiteral("postgres"), QStringLiteral("my_pg_json"));

    QCOMPARE(SqlGenerator::resolveType(entity, column, kPostgres), QStringLiteral("my_pg_json"));
}

void SqlGenPostgresTest::resolveType_nativeHintPriority()
{
    Entity entity = makeEntity("40000000-0000-0000-0000-000000000002", "e", QString());
    Column column = makeColumn("40000000-0000-0000-0000-000000000102", "c",
                               CanonicalType::Kind::Json);
    column.dialectOverrides.insert(QStringLiteral("sqlserver"), QStringLiteral("nvarchar(max)"));
    column.nativeType = DialectHint{QStringLiteral("jsonb"), QStringLiteral("postgres")};

    // Native hint matches the target engine and wins over the other-engine override.
    QCOMPARE(SqlGenerator::resolveType(entity, column, kPostgres), QStringLiteral("jsonb"));
}

void SqlGenPostgresTest::resolveType_enumUsesNativeType()
{
    Entity entity = makeEntity("40000000-0000-0000-0000-000000000003", "customer", QString());
    Column status = makeColumn("40000000-0000-0000-0000-000000000103", "status",
                               CanonicalType::Kind::Enum);
    status.type.enumValues = {QStringLiteral("a")};
    QCOMPARE(SqlGenerator::resolveType(entity, status, kPostgres),
             QStringLiteral("customer_status_enum"));
}

void SqlGenPostgresTest::createTable_multiLine()
{
    const QString table = kPostgres.createTable(
        QStringLiteral("\"public\".\"orders\""),
        {QStringLiteral("\"id\" bigint NOT NULL GENERATED BY DEFAULT AS IDENTITY"),
         QStringLiteral("\"customer_id\" bigint NOT NULL")},
        QStringLiteral("PRIMARY KEY (\"id\")"));

    const QString expected = QStringLiteral("CREATE TABLE \"public\".\"orders\" (\n"
                                            "  \"id\" bigint NOT NULL GENERATED BY DEFAULT AS IDENTITY,\n"
                                            "  \"customer_id\" bigint NOT NULL,\n"
                                            "  PRIMARY KEY (\"id\")\n"
                                            ");");
    QCOMPARE(table, expected);
}

void SqlGenPostgresTest::addForeignKeyConstraint_actions()
{
    const QString withActions = kPostgres.addForeignKeyConstraint(
        QStringLiteral("\"public\".\"orders\""), QStringLiteral("fk_orders_customer"),
        {QStringLiteral("\"customer_id\"")}, QStringLiteral("\"public\".\"customer\""),
        {QStringLiteral("\"id\"")}, QStringLiteral("CASCADE"), QStringLiteral("RESTRICT"));
    const QString expectedWithActions = QStringLiteral(
        "ALTER TABLE \"public\".\"orders\"\n"
        "  ADD CONSTRAINT \"fk_orders_customer\" FOREIGN KEY (\"customer_id\")\n"
        "  REFERENCES \"public\".\"customer\" (\"id\") ON DELETE CASCADE ON UPDATE RESTRICT;");
    QCOMPARE(withActions, expectedWithActions);

    const QString without = kPostgres.addForeignKeyConstraint(
        QStringLiteral("\"child\""), QStringLiteral("fk"), {QStringLiteral("\"p\"")},
        QStringLiteral("\"parent\""), {QStringLiteral("\"i\"")}, QString(), QString());
    const QString expectedWithout = QStringLiteral(
        "ALTER TABLE \"child\"\n"
        "  ADD CONSTRAINT \"fk\" FOREIGN KEY (\"p\")\n"
        "  REFERENCES \"parent\" (\"i\");");
    QCOMPARE(without, expectedWithout);
}

void SqlGenPostgresTest::dropTable_ifExists()
{
    QCOMPARE(kPostgres.dropTable(QStringLiteral("\"public\".\"orders\""), false),
             QStringLiteral("DROP TABLE \"public\".\"orders\";"));
    QCOMPARE(kPostgres.dropTable(QStringLiteral("\"public\".\"orders\""), true),
             QStringLiteral("DROP TABLE IF EXISTS \"public\".\"orders\";"));
}

void SqlGenPostgresTest::generate_simpleScript_orderedCorrectly()
{
    const Model model = twoTablesModel();
    const QString script = SqlGenerator().generate(model, kPostgres, SqlGenOptions{});

    const int childAt = script.indexOf(QStringLiteral("CREATE TABLE \"public\".\"child\""));
    const int parentAt = script.indexOf(QStringLiteral("CREATE TABLE \"public\".\"parent\""));
    QVERIFY(parentAt < childAt);
    const int fkAt = script.indexOf(QStringLiteral("ADD CONSTRAINT \"fk_child_parent\""));
    QVERIFY(fkAt > childAt);
}

void SqlGenPostgresTest::generate_cyclicScript_noForwardReferences()
{
    const Model model = cyclicModel();
    const QString script = SqlGenerator().generate(model, kPostgres, SqlGenOptions{});

    QVERIFY2(script.contains(QStringLiteral("ADD CONSTRAINT \"fk_a_c\"")), qPrintable(script));
    QVERIFY2(script.contains(QStringLiteral("ADD CONSTRAINT \"fk_b_a\"")), qPrintable(script));
    QVERIFY2(script.contains(QStringLiteral("ADD CONSTRAINT \"fk_c_b\"")), qPrintable(script));

    // Every CREATE TABLE must precede every ADD CONSTRAINT (no forward references).
    const int lastCreate = script.lastIndexOf(QStringLiteral("CREATE TABLE"));
    const int firstAlter = script.indexOf(QStringLiteral("ALTER TABLE"));
    QVERIFY(firstAlter == -1 || lastCreate < firstAlter);
}

QTEST_APPLESS_MAIN(SqlGenPostgresTest)

#include "tst_postgres.moc"