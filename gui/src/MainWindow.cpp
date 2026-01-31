#include "MainWindow.h"
#include "ProcessTab.h"
#include "AlertTab.h"
#include "WhitelistTab.h"
#include "SettingsTab.h"
#include "DaemonManager.h"
#include "DaemonClient.h"
#include "TrayIcon.h"
#include <QStatusBar>
#include <QCloseEvent>
#include <QJsonObject>
#include <QMessageBox>
#include <QSettings>
#include <QStyle>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabWidget(new QTabWidget(this))
    , m_processTab(new ProcessTab(this))
    , m_alertTab(new AlertTab(this))
    , m_whitelistTab(new WhitelistTab(this))
    , m_settingsTab(new SettingsTab(this))
    , m_daemonManager(new DaemonManager(this))
    , m_trayIcon(nullptr)
    , m_refreshTimer(new QTimer(this))
    , m_statusLabel(new QLabel(this))
    , m_processCountLabel(new QLabel(this))
    , m_alertCountLabel(new QLabel(this))
{
    setupUi();
    setupStatusBar();
    setupConnections();
    setupTrayIcon();
    restoreWindowState();

    // Initialize daemon manager (will auto-start daemon if needed)
    m_statusLabel->setText(tr("Starting daemon..."));
    m_daemonManager->initialize();

    // Start refresh timer (10 seconds)
    m_refreshTimer->setInterval(10000);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshData);
}

MainWindow::~MainWindow()
{
    saveWindowState();
}

DaemonClient* MainWindow::client() const
{
    return m_daemonManager->client();
}

void MainWindow::setupUi()
{
    setWindowTitle("RunawayGuard");
    resize(800, 600);

    // Use standard icons for tabs (fallback to theme icons)
    QStyle *appStyle = style();
    QIcon monitorIcon = QIcon::fromTheme("utilities-system-monitor",
        appStyle->standardIcon(QStyle::SP_ComputerIcon));
    QIcon alertIcon = QIcon::fromTheme("dialog-warning",
        appStyle->standardIcon(QStyle::SP_MessageBoxWarning));
    QIcon whitelistIcon = QIcon::fromTheme("document-properties",
        appStyle->standardIcon(QStyle::SP_FileDialogListView));
    QIcon settingsIcon = QIcon::fromTheme("preferences-system",
        appStyle->standardIcon(QStyle::SP_DialogApplyButton));

    m_tabWidget->addTab(m_processTab, monitorIcon, tr("Monitor"));
    m_tabWidget->addTab(m_alertTab, alertIcon, tr("Alerts"));
    m_tabWidget->addTab(m_whitelistTab, whitelistIcon, tr("Whitelist"));
    m_tabWidget->addTab(m_settingsTab, settingsIcon, tr("Settings"));

    setCentralWidget(m_tabWidget);
}

void MainWindow::setupConnections()
{
    // DaemonManager connections
    connect(m_daemonManager, &DaemonManager::connected, this, &MainWindow::onConnected);
    connect(m_daemonManager, &DaemonManager::disconnected, this, &MainWindow::onDisconnected);
    connect(m_daemonManager, &DaemonManager::errorOccurred, this, &MainWindow::onDaemonError);
    connect(m_daemonManager, &DaemonManager::daemonCrashed, this, &MainWindow::onDaemonCrashed);

    // DaemonClient connections (via manager)
    DaemonClient *daemonClient = m_daemonManager->client();
    connect(daemonClient, &DaemonClient::statusReceived, this, &MainWindow::onStatusReceived);
    connect(daemonClient, &DaemonClient::alertReceived, this, &MainWindow::onAlertReceived);
    connect(daemonClient, &DaemonClient::processListReceived, m_processTab, &ProcessTab::updateProcessList);
    connect(daemonClient, &DaemonClient::alertListReceived, m_alertTab, &AlertTab::updateAlertList);
    connect(daemonClient, &DaemonClient::whitelistReceived, m_whitelistTab, &WhitelistTab::updateWhitelistDisplay);

    // ProcessTab actions
    connect(m_processTab, &ProcessTab::killProcessRequested, daemonClient, &DaemonClient::requestKillProcess);
    connect(m_processTab, &ProcessTab::addWhitelistRequested, daemonClient, &DaemonClient::requestAddWhitelist);

    // WhitelistTab actions
    connect(m_whitelistTab, &WhitelistTab::addWhitelistRequested, daemonClient, &DaemonClient::requestAddWhitelist);
    connect(m_whitelistTab, &WhitelistTab::removeWhitelistRequested, daemonClient, &DaemonClient::requestRemoveWhitelist);

    // AlertTab actions
    connect(m_alertTab, &AlertTab::addWhitelistRequested, daemonClient, &DaemonClient::requestAddWhitelist);
    connect(m_alertTab, &AlertTab::killProcessRequested, daemonClient, &DaemonClient::requestKillProcess);

    // SettingsTab connections
    connect(daemonClient, &DaemonClient::configReceived, m_settingsTab, &SettingsTab::loadConfig);
    connect(m_settingsTab, &SettingsTab::configUpdateRequested, daemonClient, &DaemonClient::requestUpdateConfig);

    // TrayIcon actions (connected in setupTrayIcon after m_trayIcon is created)
}

void MainWindow::setupStatusBar()
{
    m_statusLabel->setText(tr("Disconnected"));
    m_processCountLabel->setText(tr("Processes: -"));
    m_alertCountLabel->setText(tr("Alerts: -"));

    statusBar()->addWidget(m_statusLabel);
    statusBar()->addPermanentWidget(m_processCountLabel);
    statusBar()->addPermanentWidget(m_alertCountLabel);
}

void MainWindow::setupTrayIcon()
{
    m_trayIcon = new TrayIcon(this, this);

    // Connect TrayIcon signals
    connect(m_trayIcon, &TrayIcon::pauseRequested, this, &MainWindow::onPauseMonitoring);
    connect(m_trayIcon, &TrayIcon::resumeRequested, this, &MainWindow::onResumeMonitoring);
    connect(m_trayIcon, &TrayIcon::clearAlertsRequested, this, &MainWindow::onClearAlerts);

    m_trayIcon->show();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Save window state before hiding to tray
    saveWindowState();

    // Minimize to tray instead of closing
    if (m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

void MainWindow::onConnected()
{
    m_statusLabel->setText(tr("Connected"));
    m_statusLabel->setStyleSheet("color: green;");
    m_trayIcon->setStatus(TrayIcon::Status::Normal);
    m_settingsTab->setConnected(true);
    m_daemonManager->client()->requestConfig();  // Load config on connect
    m_refreshTimer->start();
    refreshData();  // Immediate refresh on connect
}

void MainWindow::onDisconnected()
{
    m_statusLabel->setText(tr("Disconnected - Reconnecting..."));
    m_statusLabel->setStyleSheet("color: orange;");
    m_trayIcon->setStatus(TrayIcon::Status::Warning);
    m_settingsTab->setConnected(false);
    m_refreshTimer->stop();
}

void MainWindow::onDaemonError(const QString &error)
{
    m_statusLabel->setText(tr("Error: %1").arg(error));
    m_statusLabel->setStyleSheet("color: red;");
    m_trayIcon->setStatus(TrayIcon::Status::Critical);

    // Show dialog for critical errors
    if (m_daemonManager->state() == DaemonManager::State::Failed) {
        QMessageBox::critical(this, tr("Daemon Error"),
            tr("Failed to start the monitoring daemon.\n\n%1\n\n"
               "Please ensure runaway-daemon is installed correctly.").arg(error));
    }
}

void MainWindow::onDaemonCrashed()
{
    m_statusLabel->setText(tr("Daemon crashed - Restarting..."));
    m_statusLabel->setStyleSheet("color: orange;");
    m_trayIcon->setStatus(TrayIcon::Status::Warning);

    // Show tray notification
    m_trayIcon->showMessage(tr("RunawayGuard"),
        tr("The daemon crashed and is being restarted"),
        QSystemTrayIcon::Warning, 3000);
}

void MainWindow::onStatusReceived(const QJsonObject &status)
{
    int processCount = status["monitored_count"].toInt();
    int alertCount = status["alert_count"].toInt();

    m_processCountLabel->setText(tr("Processes: %1").arg(processCount));
    m_alertCountLabel->setText(tr("Alerts: %1").arg(alertCount));

    // Update tray icon with process and alert counts
    m_trayIcon->updateStatusInfo(processCount, alertCount);

    // Update tray icon status based on alert count
    if (alertCount > 0) {
        m_trayIcon->setStatus(TrayIcon::Status::Warning);
    } else {
        m_trayIcon->setStatus(TrayIcon::Status::Normal);
    }
}

void MainWindow::onAlertReceived(const QJsonObject &alert)
{
    Q_UNUSED(alert);
    // Update tray icon to warning when alert received
    m_trayIcon->setStatus(TrayIcon::Status::Warning);
    // Refresh alert list
    m_daemonManager->client()->requestAlerts();
}

void MainWindow::refreshData()
{
    DaemonClient *daemonClient = m_daemonManager->client();
    daemonClient->requestProcessList();
    daemonClient->requestAlerts();
    daemonClient->requestWhitelist();
}

void MainWindow::showStatusMessage(const QString &message, int timeout)
{
    statusBar()->showMessage(message, timeout);
}

void MainWindow::onPauseMonitoring()
{
    // Send pause request to daemon
    QJsonObject request;
    request["command"] = "pause";
    m_daemonManager->client()->sendRequest(request);
    m_trayIcon->setStatus(TrayIcon::Status::Paused);
    showStatusMessage(tr("Monitoring paused"));
}

void MainWindow::onResumeMonitoring()
{
    // Send resume request to daemon
    QJsonObject request;
    request["command"] = "resume";
    m_daemonManager->client()->sendRequest(request);
    m_trayIcon->setStatus(TrayIcon::Status::Normal);
    showStatusMessage(tr("Monitoring resumed"));
}

void MainWindow::onClearAlerts()
{
    // Send clear alerts request to daemon
    QJsonObject request;
    request["command"] = "clear_alerts";
    m_daemonManager->client()->sendRequest(request);
    m_trayIcon->setStatus(TrayIcon::Status::Normal);
    showStatusMessage(tr("Alerts cleared"));
    // Refresh alert list
    m_daemonManager->client()->requestAlerts();
}

void MainWindow::saveWindowState()
{
    QSettings settings("RunawayGuard", "GUI");
    settings.beginGroup("MainWindow");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("currentTab", m_tabWidget->currentIndex());
    settings.endGroup();
}

void MainWindow::restoreWindowState()
{
    QSettings settings("RunawayGuard", "GUI");
    settings.beginGroup("MainWindow");
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }
    if (settings.contains("windowState")) {
        restoreState(settings.value("windowState").toByteArray());
    }
    if (settings.contains("currentTab")) {
        int tabIndex = settings.value("currentTab").toInt();
        if (tabIndex >= 0 && tabIndex < m_tabWidget->count()) {
            m_tabWidget->setCurrentIndex(tabIndex);
        }
    }
    settings.endGroup();
}
