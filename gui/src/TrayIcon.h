#ifndef TRAYICON_H
#define TRAYICON_H

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

class MainWindow;

class TrayIcon : public QSystemTrayIcon
{
    Q_OBJECT

public:
    explicit TrayIcon(MainWindow *mainWindow, QObject *parent = nullptr);

    enum class Status { Normal, Warning, Critical, Paused };
    void setStatus(Status status);
    void updateStatusInfo(int processCount, int alertCount);

signals:
    void pauseRequested();
    void resumeRequested();
    void clearAlertsRequested();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onMenuAboutToShow();
    void onPauseToggled();

private:
    void setupMenu();
    void updateIcon();
    void updateTooltip();

    MainWindow *m_mainWindow;
    QMenu *m_menu;
    Status m_status;
    bool m_isPaused;
    int m_processCount;
    int m_alertCount;

    QAction *m_statusAction;
    QAction *m_pauseAction;
    QAction *m_clearAlertsAction;
};

#endif
