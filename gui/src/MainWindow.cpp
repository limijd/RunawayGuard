#include "MainWindow.h"
#include "ProcessTab.h"
#include "AlertTab.h"
#include "WhitelistTab.h"
#include "SettingsTab.h"
#include "DaemonClient.h"
#include "TrayIcon.h"
#include <QStatusBar>
#include <QCloseEvent>
#include <QJsonObject>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabWidget(new QTabWidget(this))
    , m_processTab(new ProcessTab(this))
    , m_alertTab(new AlertTab(this))
    , m_whitelistTab(new WhitelistTab(this))
    , m_settingsTab(new SettingsTab(this))
    , m_client(new DaemonClient(this))
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
    m_client->connectToDaemon();

    // Start refresh timer (10 seconds)
    m_refreshTimer->setInterval(10000);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshData);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle("RunawayGuard");
    resize(800, 600);

    m_tabWidget->addTab(m_processTab, tr("Monitor"));
    m_tabWidget->addTab(m_alertTab, tr("Alerts"));
    m_tabWidget->addTab(m_whitelistTab, tr("Whitelist"));
    m_tabWidget->addTab(m_settingsTab, tr("Settings"));

    setCentralWidget(m_tabWidget);
}

void MainWindow::setupConnections()
{
    // DaemonClient connections
    connect(m_client, &DaemonClient::connected, this, &MainWindow::onConnected);
    connect(m_client, &DaemonClient::disconnected, this, &MainWindow::onDisconnected);
    connect(m_client, &DaemonClient::statusReceived, this, &MainWindow::onStatusReceived);
    connect(m_client, &DaemonClient::alertReceived, this, &MainWindow::onAlertReceived);
    connect(m_client, &DaemonClient::processListReceived, m_processTab, &ProcessTab::updateProcessList);
    connect(m_client, &DaemonClient::alertListReceived, m_alertTab, &AlertTab::updateAlertList);
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
    m_trayIcon->show();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
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
    m_refreshTimer->start();
    refreshData();  // Immediate refresh on connect
}

void MainWindow::onDisconnected()
{
    m_statusLabel->setText(tr("Disconnected - Reconnecting..."));
    m_statusLabel->setStyleSheet("color: red;");
    m_trayIcon->setStatus(TrayIcon::Status::Critical);
    m_refreshTimer->stop();
}

void MainWindow::onStatusReceived(const QJsonObject &status)
{
    int processCount = status["monitored_count"].toInt();
    int alertCount = status["alert_count"].toInt();

    m_processCountLabel->setText(tr("Processes: %1").arg(processCount));
    m_alertCountLabel->setText(tr("Alerts: %1").arg(alertCount));

    // Update tray icon based on alert count
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
    m_client->requestAlerts();
}

void MainWindow::refreshData()
{
    m_client->requestProcessList();
    m_client->requestAlerts();
}
