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

const QString kThemeSetting = QStringLiteral("appearanceTheme");
const QString kSoftBlueTheme = QStringLiteral("SoftBlue");
const QString kDefaultTheme = kSoftBlueTheme;

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

// Calm, blue-tinted dark palette. The accent is a soft light blue, and text drawn on
// top of it is dark ink so both letter/background pairs keep a strong contrast.
QPalette softBluePalette()
{
    const QColor window(43, 49, 56);       // #2b3138
    const QColor base(51, 59, 68);         // #333b44
    const QColor alternate(60, 70, 80);    // #3c4650
    const QColor text(232, 238, 245);      // #e8eef5
    const QColor button(57, 66, 75);       // #39424b
    const QColor softBlue(158, 199, 232);  // #9ec7e8
    const QColor ink(27, 39, 51);          // #1b2733

    QPalette palette;
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, alternate);
    palette.setColor(QPalette::ToolTipBase, base);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Link, softBlue);
    palette.setColor(QPalette::LinkVisited, softBlue.darker(115));
    palette.setColor(QPalette::Highlight, softBlue);
    palette.setColor(QPalette::HighlightedText, ink);
    palette.setColor(QPalette::Mid, QColor(107, 120, 133));     // borders
    palette.setColor(QPalette::Midlight, QColor(85, 96, 107));
    palette.setColor(QPalette::Dark, QColor(35, 41, 48));
    palette.setColor(QPalette::Light, QColor(74, 85, 97));
    palette.setColor(QPalette::Shadow, QColor(27, 32, 38));
    palette.setColor(QPalette::PlaceholderText, QColor(154, 167, 180));

    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 139, 150));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 139, 150));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 139, 150));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(88, 99, 110));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(201, 211, 220));
    return palette;
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
    return {kSoftBlueTheme, QString(), QStringLiteral("DarkBlue"), QStringLiteral("DarkGreen"),
            QStringLiteral("DarkMaroon"), QStringLiteral("DarkPurple")};
}

QString themeDisplayName(const QString &themeId)
{
    if (themeId == kSoftBlueTheme)
        return QStringLiteral("Soft Blue (default)");
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

    if (themeId == kSoftBlueTheme) {
        // Palette-only: standard Fusion indicators, calm colours, no saturated accents.
        app->setStyleSheet(QString());
        app->setPalette(softBluePalette());
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
