#pragma once

#include "SqlDialect.h"

#include <QList>
#include <QString>

namespace bb {

// The target engines beDBug can generate SQL for. Dialects are process-wide singletons
// with static lifetime, so returned pointers always remain valid.
class DialectRegistry
{
public:
    // Registered dialects in display order (PostgreSQL first).
    static QList<const SqlDialect *> all();

    // The dialect with this id, or nullptr when unknown.
    static const SqlDialect *find(const QString &id);

    // PostgreSQL, the dialect used when no explicit target has been chosen.
    static const SqlDialect *fallback();
};

} // namespace bb
