#pragma once

#include <QString>
#include <QStringList>

class QApplication;

namespace bb {

// Dark themes vendored from github.com/keshav-sahu7/qt-dark-theme, plus the built-in
// calm "Soft Blue" default. The empty string means "system default" (native palette).
QStringList availableThemeIds();
QString themeDisplayName(const QString &themeId);

// Reads the persisted theme id (QSettings key "appearanceTheme", default "SoftBlue").
QString currentThemeId();

// Applies the given theme: installs the theme stylesheet and a matching palette.
// Custom-painted items (canvas, tables, edges) read the palette, so the palette
// is what actually makes the whole UI dark.
void applyTheme(QApplication *app, const QString &themeId);
void applyCurrentTheme(QApplication *app);

} // namespace bb
