#include "Id.h"

namespace bb {

Id Id::create()
{
    Id id;
    id.m_uuid = QUuid::createUuid();
    return id;
}

Id Id::fromString(const QString &text, bool *ok)
{
    Id id;
    if (ok)
        *ok = false;

    id.m_uuid = QUuid(text);
    if (!id.m_uuid.isNull() && ok)
        *ok = true;

    return id;
}

bool Id::isNull() const
{
    return m_uuid.isNull();
}

QString Id::toString() const
{
    return m_uuid.toString(QUuid::WithoutBraces);
}

} // namespace bb