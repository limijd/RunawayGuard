#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>

class ProcessTab;
class AlertTab;
class WhitelistTab;
class SettingsTab;
class DaemonClient;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void setupUi();
    void setupStatusBar();

    QTabWidget *m_tabWidget;
    ProcessTab *m_processTab;
    AlertTab *m_alertTab;
    WhitelistTab *m_whitelistTab;
    SettingsTab *m_settingsTab;
    DaemonClient *m_client;
};

#endif
