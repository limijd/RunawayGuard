#ifndef TRAYICON_H
#define TRAYICON_H

#include <QSystemTrayIcon>
#include <QMenu>

class MainWindow;

class TrayIcon : public QSystemTrayIcon
{
    Q_OBJECT

public:
    explicit TrayIcon(MainWindow *mainWindow, QObject *parent = nullptr);

    enum class Status { Normal, Warning, Critical };
    void setStatus(Status status);

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void setupMenu();
    void updateIcon();
    void updateTooltip();

    MainWindow *m_mainWindow;
    QMenu *m_menu;
    Status m_status;
};

#endif
