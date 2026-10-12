#pragma once

#include <QString>
#include <QUuid>

namespace bb {

// Stable opaque identifier for every model object, backed by a QUuid.
class Id
{
public:
    Id() = default;              // null id
    static Id create();          // new random id
    static Id fromString(const QString &text, bool *ok = nullptr);

    bool isNull() const;
    QString toString() const;    // UUID without braces

    bool operator==(const Id &other) const { return m_uuid == other.m_uuid; }
    bool operator!=(const Id &other) const { return !(*this == other); }
    bool operator<(const Id &other) const { return m_uuid < other.m_uuid; }

    friend size_t qHash(const Id &id, size_t seed)
    {
        return qHash(id.m_uuid, seed);
    }

private:
    QUuid m_uuid;
};

} // namespace bb

Q_DECLARE_TYPEINFO(bb::Id, Q_RELOCATABLE_TYPE);