#include "Version.h"

namespace bb {

QString versionString()
{
    return QStringLiteral("%1.%2.%3").arg(versionMajor()).arg(versionMinor()).arg(versionPatch());
}

int versionMajor()
{
    return 0;
}

int versionMinor()
{
    return 0;
}

int versionPatch()
{
    return 1;
}

} // namespace bb