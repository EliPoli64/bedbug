#include "MainWindow.h"

#include <QApplication>

using namespace bb;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("bedbug"));
    app.setApplicationVersion(QStringLiteral("0.0.1"));
    app.setOrganizationName(QStringLiteral("bedbug"));

    MainWindow window;
    window.show();

    return app.exec();
}