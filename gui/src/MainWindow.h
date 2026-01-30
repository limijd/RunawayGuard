#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTimer>
#include <QLabel>

class ProcessTab;
class AlertTab;
class WhitelistTab;
class SettingsTab;
class DaemonClient;
class TrayIcon;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    DaemonClient* client() const { return m_client; }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onConnected();
    void onDisconnected();
    void onStatusReceived(const QJsonObject &status);
    void onAlertReceived(const QJsonObject &alert);
    void refreshData();

private:
    void setupUi();
    void setupConnections();
    void setupStatusBar();
    void setupTrayIcon();

    QTabWidget *m_tabWidget;
    ProcessTab *m_processTab;
    AlertTab *m_alertTab;
    WhitelistTab *m_whitelistTab;
    SettingsTab *m_settingsTab;
    DaemonClient *m_client;
    TrayIcon *m_trayIcon;
    QTimer *m_refreshTimer;
    QLabel *m_statusLabel;
    QLabel *m_processCountLabel;
    QLabel *m_alertCountLabel;
};

#endif
