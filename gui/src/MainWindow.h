#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTimer>
#include <QLabel>
#include <QSettings>

class ProcessTab;
class AlertTab;
class WhitelistTab;
class SettingsTab;
class DaemonManager;
class DaemonClient;
class TrayIcon;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    DaemonManager* daemonManager() const { return m_daemonManager; }
    DaemonClient* client() const;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onConnected();
    void onDisconnected();
    void onStatusReceived(const QJsonObject &status);
    void onAlertReceived(const QJsonObject &alert);
    void onDaemonError(const QString &error);
    void onDaemonCrashed();
    void refreshData();
    void showStatusMessage(const QString &message, int timeout = 3000);
    void onPauseMonitoring();
    void onResumeMonitoring();
    void onClearAlerts();

private:
    void setupUi();
    void setupConnections();
    void setupStatusBar();
    void setupTrayIcon();
    void saveWindowState();
    void restoreWindowState();

    QTabWidget *m_tabWidget;
    ProcessTab *m_processTab;
    AlertTab *m_alertTab;
    WhitelistTab *m_whitelistTab;
    SettingsTab *m_settingsTab;
    DaemonManager *m_daemonManager;
    TrayIcon *m_trayIcon;
    QTimer *m_refreshTimer;
    QLabel *m_statusLabel;
    QLabel *m_processCountLabel;
    QLabel *m_alertCountLabel;
};

#endif
