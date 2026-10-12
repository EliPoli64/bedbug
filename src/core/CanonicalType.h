#pragma once

#include <QString>
#include <QStringList>

namespace bb {

// Engine-neutral data type. The model stores types in this canonical form and maps
// them to a concrete dialect at generation time (see PLAN.md §7).
struct CanonicalType
{
    enum class Kind {
        Int,
        BigInt,
        SmallInt,
        Varchar,
        Char,
        Text,
        Boolean,
        Date,
        Time,
        Timestamp,
        Decimal,
        Float,
        Double,
        Blob,
        Json,
        Uuid,
        Enum,
    };

    Kind kind = Kind::Varchar;
    int length = 0;           // varchar(n) / char(n)
    int precision = 0;        // decimal(p, s)
    int scale = 0;
    QStringList enumValues;

    bool operator==(const CanonicalType &) const = default;
    bool operator!=(const CanonicalType &) const = default;

    // "BigInt" <-> Kind::BigInt. On unknown input returns false and Kind::Varchar.
    static QString kindToString(Kind kind);
    static Kind stringToKind(const QString &text, bool *ok = nullptr);

    // Basic sanity: sized kinds need a length, decimals need precision, enums need values.
    bool isValid() const;
};

} // namespace bb