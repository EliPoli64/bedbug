#include "MainWindow.h"

#include <QStatusBar>

namespace {
constexpr int kDefaultWidth = 1000;
constexpr int kDefaultHeight = 700;
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("bedbug"));
    resize(kDefaultWidth, kDefaultHeight);
    statusBar()->showMessage(tr("Ready"));
}