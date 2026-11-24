#include "SqlGenerator.h"
#include "JsonSerializer.h"
#include "dialects/Postgres.h"
#include "dialects/SqlServer.h"

#include <QtTest>

namespace {

using namespace bb;

const SqlServerDialect kSqlServer;
const PostgresDialect kPostgres;

Id mid(const char *text)
{
    bool ok = false;
    const Id id = Id::fromString(QLatin1String(text), &ok);
    Q_ASSERT(ok);
    return id;
}

Column makeColumn(const char *idText, const QString &name, CanonicalType::Kind kind)
{
    Column column;
    column.id = mid(idText);
    column.name = name;
    column.type.kind = kind;
    return column;
}

} // namespace

class SqlServerTest : public QObject
{
    Q_OBJECT

private slots:
    void quoteIdent_escapesBrackets();
    void quoteLiteral_usesUnicodePrefix();
    void mapType_coversAllKinds();
    void mapType_narrowsOversizedStrings();
    void columnDefinition_identity();
    void dropTable_ifExists();
    void addForeignKeyConstraint_normalizesRestrict();
    void auditTypes_reportsOnlyReviewableMappings();
    void auditTypes_silentWhenOverrideOrNativeHintPresent();
    void auditTypes_postgresHasNoAdvisoriesForItsExactTypes();
    void generate_usesBracketQuoting();
    void acceptedOverrides_surviveJsonRoundTripAndKeepSqlStable();
};

void SqlServerTest::quoteIdent_escapesBrackets()
{
    QCOMPARE(kSqlServer.quoteIdent(QStringLiteral("customer")), QStringLiteral("[customer]"));
    QCOMPARE(kSqlServer.quoteIdent(QStringLiteral("weird]name")),
             QStringLiteral("[weird]]name]"));
}

void SqlServerTest::quoteLiteral_usesUnicodePrefix()
{
    QCOMPARE(kSqlServer.quoteLiteral(QStringLiteral("plain")), QStringLiteral("N'plain'"));
    QCOMPARE(kSqlServer.quoteLiteral(QStringLiteral("O'Brien")), QStringLiteral("N'O''Brien'"));
}

void SqlServerTest::mapType_coversAllKinds()
{
    struct Case
    {
        CanonicalType::Kind kind;
        const char *expected;
        Fidelity fidelity;
    };

    const Case cases[] = {
        {CanonicalType::Kind::Int, "int", Fidelity::Exact},
        {CanonicalType::Kind::BigInt, "bigint", Fidelity::Exact},
        {CanonicalType::Kind::SmallInt, "smallint", Fidelity::Exact},
        {CanonicalType::Kind::Text, "nvarchar(max)", Fidelity::Widened},
        {CanonicalType::Kind::Boolean, "bit", Fidelity::Exact},
        {CanonicalType::Kind::Date, "date", Fidelity::Exact},
        {CanonicalType::Kind::Time, "time", Fidelity::Exact},
        {CanonicalType::Kind::Timestamp, "datetime2", Fidelity::Lossy},
        {CanonicalType::Kind::Float, "real", Fidelity::Exact},
        {CanonicalType::Kind::Double, "float", Fidelity::Exact},
        {CanonicalType::Kind::Blob, "varbinary(max)", Fidelity::Exact},
        {CanonicalType::Kind::Json, "nvarchar(max)", Fidelity::Unsupported},
        {CanonicalType::Kind::Uuid, "uniqueidentifier", Fidelity::Exact},
    };

    for (const Case &c : cases) {
        CanonicalType type;
        type.kind = c.kind;
        const TypeMapping mapping = kSqlServer.mapType(type);
        QCOMPARE(mapping.sqlType, QString::fromLatin1(c.expected));
        QVERIFY2(mapping.fidelity == c.fidelity, c.expected);
    }

    CanonicalType varchar;
    varchar.kind = CanonicalType::Kind::Varchar;
    varchar.length = 100;
    QCOMPARE(kSqlServer.mapType(varchar).sqlType, QStringLiteral("nvarchar(100)"));

    CanonicalType chr;
    chr.kind = CanonicalType::Kind::Char;
    chr.length = 2;
    QCOMPARE(kSqlServer.mapType(chr).sqlType, QStringLiteral("nchar(2)"));

    CanonicalType decimalType;
    decimalType.kind = CanonicalType::Kind::Decimal;
    decimalType.precision = 12;
    decimalType.scale = 3;
    QCOMPARE(kSqlServer.mapType(decimalType).sqlType, QStringLiteral("decimal(12,3)"));

    CanonicalType enumType;
    enumType.kind = CanonicalType::Kind::Enum;
    enumType.enumValues = {QStringLiteral("a")};
    QCOMPARE(kSqlServer.mapType(enumType).sqlType, QStringLiteral("nvarchar(255)"));
    QVERIFY(kSqlServer.mapType(enumType).fidelity == Fidelity::Lossy);
}

void SqlServerTest::mapType_narrowsOversizedStrings()
{
    CanonicalType huge;
    huge.kind = CanonicalType::Kind::Varchar;
    huge.length = 10000;
    const TypeMapping mapping = kSqlServer.mapType(huge);
    QCOMPARE(mapping.sqlType, QStringLiteral("nvarchar(4000)"));
    QVERIFY(mapping.fidelity == Fidelity::Narrowed);
    QCOMPARE(mapping.suggestedOverride, QStringLiteral("nvarchar(max)"));
}

void SqlServerTest::columnDefinition_identity()
{
    Column identity = makeColumn("50000000-0000-0000-0000-000000000001", "id",
                                 CanonicalType::Kind::BigInt);
    identity.nullable = false;
    identity.autoIncrement = true;
    identity.unique = true;
    QCOMPARE(kSqlServer.columnDefinition(identity, QStringLiteral("bigint")),
             QStringLiteral("[id] bigint NOT NULL IDENTITY(1,1) UNIQUE"));

    Column withDefault = makeColumn("50000000-0000-0000-0000-000000000002", "created_at",
                                    CanonicalType::Kind::Timestamp);
    withDefault.nullable = false;
    withDefault.defaultValue = QStringLiteral("GETDATE()");
    QCOMPARE(kSqlServer.columnDefinition(withDefault, QStringLiteral("datetime2")),
             QStringLiteral("[created_at] datetime2 NOT NULL DEFAULT GETDATE()"));
}

void SqlServerTest::dropTable_ifExists()
{
    QCOMPARE(kSqlServer.dropTable(QStringLiteral("[dbo].[orders]"), false),
             QStringLiteral("DROP TABLE [dbo].[orders];"));
    QCOMPARE(kSqlServer.dropTable(QStringLiteral("[dbo].[orders]"), true),
             QStringLiteral("DROP TABLE IF EXISTS [dbo].[orders];"));
}

void SqlServerTest::addForeignKeyConstraint_normalizesRestrict()
{
    const QString statement = kSqlServer.addForeignKeyConstraint(
        QStringLiteral("[dbo].[child]"), QStringLiteral("fk_child_parent"),
        {QStringLiteral("[parent_id]")}, QStringLiteral("[dbo].[parent]"),
        {QStringLiteral("[id]")}, QStringLiteral("CASCADE"), QStringLiteral("RESTRICT"));

    QVERIFY2(statement.contains(QStringLiteral("ON DELETE CASCADE")), qPrintable(statement));
    QVERIFY2(statement.contains(QStringLiteral("ON UPDATE NO ACTION")), qPrintable(statement));
    QVERIFY2(!statement.contains(QStringLiteral("RESTRICT")), qPrintable(statement));
}

void SqlServerTest::auditTypes_reportsOnlyReviewableMappings()
{
    Entity entity;
    entity.id = mid("60000000-0000-0000-0000-000000000001");
    entity.name = QStringLiteral("customer");

    entity.columns = {
        makeColumn("60000000-0000-0000-0000-000000000101", "id", CanonicalType::Kind::BigInt),
        makeColumn("60000000-0000-0000-0000-000000000102", "payload", CanonicalType::Kind::Json),
        makeColumn("60000000-0000-0000-0000-000000000103", "seen_at",
                   CanonicalType::Kind::Timestamp),
        makeColumn("60000000-0000-0000-0000-000000000104", "kind", CanonicalType::Kind::Enum),
        makeColumn("60000000-0000-0000-0000-000000000105", "uuid", CanonicalType::Kind::Uuid),
    };

    Model model;
    model.entities = {entity};

    const QList<TypeAdvisory> advisories = SqlGenerator::auditTypes(model, kSqlServer);
    QCOMPARE(advisories.size(), 3);

    QCOMPARE(advisories.at(0).columnName, QStringLiteral("payload"));
    QVERIFY(advisories.at(0).fidelity == Fidelity::Unsupported);
    QCOMPARE(advisories.at(0).resolvedType, QStringLiteral("nvarchar(max)"));

    QCOMPARE(advisories.at(1).columnName, QStringLiteral("seen_at"));
    QVERIFY(advisories.at(1).fidelity == Fidelity::Lossy);

    QCOMPARE(advisories.at(2).columnName, QStringLiteral("kind"));
    QVERIFY(advisories.at(2).fidelity == Fidelity::Lossy);
}

void SqlServerTest::auditTypes_silentWhenOverrideOrNativeHintPresent()
{
    Entity entity;
    entity.id = mid("60000000-0000-0000-0000-000000000002");
    entity.name = QStringLiteral("customer");

    Column overridden = makeColumn("60000000-0000-0000-0000-000000000201", "payload",
                                   CanonicalType::Kind::Json);
    overridden.dialectOverrides.insert(QStringLiteral("sqlserver"),
                                       QStringLiteral("nvarchar(max)"));

    Column native = makeColumn("60000000-0000-0000-0000-000000000202", "seen_at",
                               CanonicalType::Kind::Timestamp);
    native.nativeType = DialectHint{QStringLiteral("datetime2"), QStringLiteral("sqlserver")};

    entity.columns = {overridden, native};
    Model model;
    model.entities = {entity};

    QVERIFY(SqlGenerator::auditTypes(model, kSqlServer).isEmpty());
}

void SqlServerTest::auditTypes_postgresHasNoAdvisoriesForItsExactTypes()
{
    Entity entity;
    entity.id = mid("60000000-0000-0000-0000-000000000003");
    entity.name = QStringLiteral("customer");

    Column status = makeColumn("60000000-0000-0000-0000-000000000301", "status",
                               CanonicalType::Kind::Enum);
    status.type.enumValues = {QStringLiteral("a"), QStringLiteral("b")};

    entity.columns = {
        makeColumn("60000000-0000-0000-0000-000000000302", "payload", CanonicalType::Kind::Json),
        makeColumn("60000000-0000-0000-0000-000000000303", "uuid", CanonicalType::Kind::Uuid),
        makeColumn("60000000-0000-0000-0000-000000000304", "seen_at",
                   CanonicalType::Kind::Timestamp),
        status,
    };

    Model model;
    model.entities = {entity};

    QVERIFY(SqlGenerator::auditTypes(model, kPostgres).isEmpty());
}

void SqlServerTest::generate_usesBracketQuoting()
{
    Entity parent;
    parent.id = mid("70000000-0000-0000-0000-000000000001");
    parent.name = QStringLiteral("parent");
    parent.schema = QStringLiteral("dbo");
    Column id = makeColumn("70000000-0000-0000-0000-000000000101", "id",
                           CanonicalType::Kind::BigInt);
    id.nullable = false;
    id.primaryKey = true;
    id.autoIncrement = true;
    parent.columns = {id};

    Model model;
    model.entities = {parent};

    const QString script = SqlGenerator().generate(model, kSqlServer);
    QVERIFY2(script.contains(QStringLiteral("CREATE TABLE [dbo].[parent]")), qPrintable(script));
    QVERIFY2(script.contains(QStringLiteral("[id] bigint NOT NULL IDENTITY(1,1)")),
             qPrintable(script));
    QVERIFY2(script.contains(QStringLiteral("PRIMARY KEY ([id])")), qPrintable(script));
    QVERIFY2(script.contains(QStringLiteral("Target engine: SQL Server")), qPrintable(script));
}

void SqlServerTest::acceptedOverrides_surviveJsonRoundTripAndKeepSqlStable()
{
    Entity entity;
    entity.id = mid("80000000-0000-0000-0000-000000000001");
    entity.name = QStringLiteral("event");

    Column payload = makeColumn("80000000-0000-0000-0000-000000000101", "payload",
                                CanonicalType::Kind::Json);
    payload.dialectOverrides.insert(QStringLiteral("sqlserver"), QStringLiteral("nvarchar(max)"));
    entity.columns = {payload};

    Model model;
    model.entities = {entity};

    const QString before = SqlGenerator().generate(model, kSqlServer);
    const Model reloaded = JsonSerializer::fromJson(JsonSerializer::toJson(model));
    const QString after = SqlGenerator().generate(reloaded, kSqlServer);

    QCOMPARE(after, before);
    QVERIFY(SqlGenerator::auditTypes(reloaded, kSqlServer).isEmpty());
}

QTEST_APPLESS_MAIN(SqlServerTest)

#include "tst_sqlserver.moc"
