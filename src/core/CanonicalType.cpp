#include "CanonicalType.h"

#include <array>

namespace bb {

namespace {

struct KindName
{
    CanonicalType::Kind kind;
    const char *name;
};

// Order matches CanonicalType::Kind.
constexpr std::array kKindNames = {
    KindName{CanonicalType::Kind::Int,       "Int"},
    KindName{CanonicalType::Kind::BigInt,    "BigInt"},
    KindName{CanonicalType::Kind::SmallInt,  "SmallInt"},
    KindName{CanonicalType::Kind::Varchar,   "Varchar"},
    KindName{CanonicalType::Kind::Char,      "Char"},
    KindName{CanonicalType::Kind::Text,      "Text"},
    KindName{CanonicalType::Kind::Boolean,   "Boolean"},
    KindName{CanonicalType::Kind::Date,      "Date"},
    KindName{CanonicalType::Kind::Time,      "Time"},
    KindName{CanonicalType::Kind::Timestamp, "Timestamp"},
    KindName{CanonicalType::Kind::Decimal,   "Decimal"},
    KindName{CanonicalType::Kind::Float,     "Float"},
    KindName{CanonicalType::Kind::Double,    "Double"},
    KindName{CanonicalType::Kind::Blob,      "Blob"},
    KindName{CanonicalType::Kind::Json,      "Json"},
    KindName{CanonicalType::Kind::Uuid,      "Uuid"},
    KindName{CanonicalType::Kind::Enum,      "Enum"},
};

} // namespace

QString CanonicalType::kindToString(Kind kind)
{
    for (const KindName &entry : kKindNames) {
        if (entry.kind == kind)
            return QString::fromLatin1(entry.name);
    }
    return {};
}

CanonicalType::Kind CanonicalType::stringToKind(const QString &text, bool *ok)
{
    if (ok)
        *ok = false;

    for (const KindName &entry : kKindNames) {
        if (QString::fromLatin1(entry.name) == text) {
            if (ok)
                *ok = true;
            return entry.kind;
        }
    }
    return Kind::Varchar;
}

bool CanonicalType::isValid() const
{
    switch (kind) {
    case Kind::Varchar:
    case Kind::Char:
        return length > 0;
    case Kind::Decimal:
        return precision > 0 && scale >= 0 && scale <= precision;
    case Kind::Enum:
        return !enumValues.isEmpty();
    default:
        return true;
    }
}

QString CanonicalType::displayName() const
{
    switch (kind) {
    case Kind::Varchar:
    case Kind::Char: {
        const QString base = kindToString(kind).toLower();
        return length > 0 ? base + QStringLiteral("(%1)").arg(length) : base;
    }
    case Kind::Decimal:
        return precision > 0 ? QStringLiteral("decimal(%1,%2)").arg(precision).arg(scale)
                             : QStringLiteral("decimal");
    case Kind::Enum:
        return QStringLiteral("enum");
    default:
        return kindToString(kind).toLower();
    }
}

} // namespace bb