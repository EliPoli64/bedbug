#include "DialectRegistry.h"

#include "dialects/MySql.h"
#include "dialects/Postgres.h"
#include "dialects/SqlServer.h"

namespace bb {

namespace {

const SqlDialect *postgres()
{
    static const PostgresDialect dialect;
    return &dialect;
}

const SqlDialect *mysql()
{
    static const MySqlDialect dialect;
    return &dialect;
}

const SqlDialect *sqlServer()
{
    static const SqlServerDialect dialect;
    return &dialect;
}

} // namespace

QList<const SqlDialect *> DialectRegistry::all()
{
    return {postgres(), mysql(), sqlServer()};
}

const SqlDialect *DialectRegistry::find(const QString &id)
{
    for (const SqlDialect *dialect : all()) {
        if (dialect->id() == id)
            return dialect;
    }
    return nullptr;
}

const SqlDialect *DialectRegistry::fallback()
{
    return postgres();
}

} // namespace bb
