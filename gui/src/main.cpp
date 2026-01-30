#include <QApplication>
#include <QStyleHints>
#include "MainWindow.h"
#include "TrayIcon.h"

int main(int argc, char *argv[])
{
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName("RunawayGuard");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("RunawayGuard");
    app.setQuitOnLastWindowClosed(false);

    MainWindow mainWindow;
    TrayIcon trayIcon(&mainWindow);
    trayIcon.show();

    return app.exec();
}
