#include "MainWindow.h"
#include "ProcessTab.h"
#include "AlertTab.h"
#include "WhitelistTab.h"
#include "SettingsTab.h"
#include "DaemonClient.h"
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabWidget(new QTabWidget(this))
    , m_processTab(new ProcessTab(this))
    , m_alertTab(new AlertTab(this))
    , m_whitelistTab(new WhitelistTab(this))
    , m_settingsTab(new SettingsTab(this))
    , m_client(new DaemonClient(this))
{
    setupUi();
    setupStatusBar();
    m_client->connectToDaemon();
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

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(tr("Connecting to daemon..."));
}
