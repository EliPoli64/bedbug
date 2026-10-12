#pragma once

#include <QString>

namespace bb {

// Application version, mirrored from the CMake project version.
QString versionString();

int versionMajor();
int versionMinor();
int versionPatch();

} // namespace bb