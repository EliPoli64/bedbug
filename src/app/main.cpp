#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

using namespace bb;

namespace {

QIcon bedbugIcon()
{
    QSvgRenderer renderer(QStringLiteral(":/assets/logo.svg"));
    QIcon icon;

    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        renderer.render(&painter);

        icon.addPixmap(pixmap);
    }

    return icon;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("bedbug"));
    app.setApplicationVersion(QStringLiteral("0.0.1"));
    app.setOrganizationName(QStringLiteral("bedbug"));

    // Dark by default; the theme is selectable in Settings → Appearance.
    applyCurrentTheme(&app);

    app.setWindowIcon(bedbugIcon());

    MainWindow window;
    window.show();

    return app.exec();
}
