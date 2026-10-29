#include "Theme.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QtGlobal>

// Resources live in the static bb_editor library; initialize them at load time so
// they are available even when no other resource code runs first. Q_INIT_RESOURCE
// must be expanded at global scope (it declares the generated init functions there).
static void bbInitThemeResources()
{
    Q_INIT_RESOURCE(themes);
    Q_INIT_RESOURCE(theme_icons);
}
Q_CONSTRUCTOR_FUNCTION(bbInitThemeResources)

namespace bb {

namespace {

const QString kThemeSetting = QStringLiteral("theme");
const QString kDefaultTheme = QStringLiteral("DarkBlue");

QColor accentFor(const QString &themeId)
{
    if (themeId == QLatin1String("DarkGreen"))
        return QColor(31, 163, 70);
    if (themeId == QLatin1String("DarkMaroon"))
        return QColor(195, 33, 72);
    if (themeId == QLatin1String("DarkPurple"))
        return QColor(121, 80, 245);
    return QColor(34, 34, 255);   // DarkBlue
}

QPalette darkPalette(const QColor &accent)
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(36, 36, 36));
    palette.setColor(QPalette::WindowText, QColor(232, 232, 232));
    palette.setColor(QPalette::Base, QColor(46, 46, 46));
    palette.setColor(QPalette::AlternateBase, QColor(56, 56, 56));
    palette.setColor(QPalette::ToolTipBase, QColor(46, 46, 46));
    palette.setColor(QPalette::ToolTipText, QColor(232, 232, 232));
    palette.setColor(QPalette::Text, QColor(232, 232, 232));
    palette.setColor(QPalette::Button, QColor(52, 52, 52));
    palette.setColor(QPalette::ButtonText, QColor(232, 232, 232));
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Link, accent);
    palette.setColor(QPalette::LinkVisited, accent.darker());
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Mid, QColor(90, 90, 90));
    palette.setColor(QPalette::Midlight, QColor(70, 70, 70));
    palette.setColor(QPalette::Dark, QColor(25, 25, 25));
    palette.setColor(QPalette::Light, QColor(80, 80, 80));
    palette.setColor(QPalette::Shadow, QColor(15, 15, 15));
    palette.setColor(QPalette::PlaceholderText, QColor(150, 150, 150));

    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(130, 130, 130));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(130, 130, 130));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(130, 130, 130));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(180, 180, 180));
    return palette;
}

} // namespace

QStringList availableThemeIds()
{
    return {QString(), QStringLiteral("DarkBlue"), QStringLiteral("DarkGreen"),
            QStringLiteral("DarkMaroon"), QStringLiteral("DarkPurple")};
}

QString themeDisplayName(const QString &themeId)
{
    if (themeId.isEmpty())
        return QStringLiteral("System (light)");
    if (themeId == QLatin1String("DarkBlue"))
        return QStringLiteral("Dark Blue");
    if (themeId == QLatin1String("DarkGreen"))
        return QStringLiteral("Dark Green");
    if (themeId == QLatin1String("DarkMaroon"))
        return QStringLiteral("Dark Maroon");
    if (themeId == QLatin1String("DarkPurple"))
        return QStringLiteral("Dark Purple");
    return themeId;
}

QString currentThemeId()
{
    return QSettings().value(kThemeSetting, kDefaultTheme).toString();
}

void applyTheme(QApplication *app, const QString &themeId)
{
    if (!app)
        return;

    // Fusion honours QPalette consistently on every platform, which matters for the
    // custom-painted canvas and table items.
    app->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    if (themeId.isEmpty()) {
        app->setStyleSheet(QString());
        app->setPalette(app->style()->standardPalette());
        return;
    }

    const QString path = QStringLiteral(":/%1.qss").arg(themeId);
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        app->setStyleSheet(QString::fromUtf8(file.readAll()));

    app->setPalette(darkPalette(accentFor(themeId)));
}

void applyCurrentTheme(QApplication *app)
{
    applyTheme(app, currentThemeId());
}

} // namespace bb
