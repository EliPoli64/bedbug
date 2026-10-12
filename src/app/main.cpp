#include "MainWindow.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

using namespace bb;

namespace {

QIcon bedbugIcon()
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QApplication::palette().color(QPalette::Highlight));
        painter.drawRoundedRect(QRectF(2, 2, 60, 60), 14, 14);
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPointSize(26);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("b"));
    }
    return QIcon(pixmap);
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("bedbug"));
    app.setApplicationVersion(QStringLiteral("0.0.1"));
    app.setOrganizationName(QStringLiteral("bedbug"));
    app.setWindowIcon(bedbugIcon());

    MainWindow window;
    window.show();

    return app.exec();
}