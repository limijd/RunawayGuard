#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStyleHints>
#include "MainWindow.h"
#include "TrayIcon.h"

static const QString SERVER_NAME = "RunawayGuard-SingleInstance";

bool isAlreadyRunning()
{
    QLocalSocket socket;
    socket.connectToServer(SERVER_NAME);
    if (socket.waitForConnected(500)) {
        // Another instance is running - send "show" command
        socket.write("show");
        socket.waitForBytesWritten(500);
        socket.disconnectFromServer();
        return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName("RunawayGuard");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("RunawayGuard");
    app.setQuitOnLastWindowClosed(false);

    // Check if another instance is running
    if (isAlreadyRunning()) {
        return 0;  // Exit - the other instance will show itself
    }

    // Create local server for single-instance communication
    QLocalServer::removeServer(SERVER_NAME);  // Clean up stale socket
    QLocalServer server;
    if (!server.listen(SERVER_NAME)) {
        qWarning("Failed to create single-instance server");
    }

    MainWindow mainWindow;
    TrayIcon trayIcon(&mainWindow);
    trayIcon.show();

    // Handle incoming connections from other instances
    QObject::connect(&server, &QLocalServer::newConnection, [&]() {
        QLocalSocket *client = server.nextPendingConnection();
        if (client) {
            client->waitForReadyRead(500);
            QByteArray data = client->readAll();
            if (data == "show") {
                mainWindow.show();
                mainWindow.raise();
                mainWindow.activateWindow();
            }
            client->deleteLater();
        }
    });

    return app.exec();
}
